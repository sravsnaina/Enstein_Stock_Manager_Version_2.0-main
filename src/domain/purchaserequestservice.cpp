#include "domain/purchaserequestservice.h"

#include "core/appsettings.h"
#include "core/dbmanager.h"
#include "core/dbschema.h"
#include "domain/itemmasterservice.h"

#include <QDateTime>
#include <QDebug>

void PurchaseRequestService::load()
{
    m_requests.clear();
    if (!m_db) return;
    m_requests = dbRowsToApp(kPrFields, m_db->selectAll("purchase_requests", "pr_no"));

    for (QVariantMap &pr : m_requests) {
        if (pr["status"].toString().trimmed().isEmpty())
            pr["status"] = "Pending";
    }

    loadItems();

    qDebug() << "Loaded" << m_requests.size() << "purchase requests ("
             << m_prItems.size() << "line items )";
}

void PurchaseRequestService::save()
{
    if (!m_db) return;
    m_db->replaceAll("purchase_requests", appRowsToDb(kPrFields, m_requests));
}

void PurchaseRequestService::loadItems()
{
    m_prItems.clear();
    if (!m_db) return;
    m_prItems = dbRowsToApp(kPrItemFields, m_db->selectAll("pr_items", "id"));

    for (QVariantMap &pr : m_requests)
        recalcHeader(pr);
}

void PurchaseRequestService::recalcHeader(QVariantMap &pr)
{
    const QString prNo = pr["prNo"].toString();
    int count = 0, totalQty = 0;
    double estimatedValue = 0.0;
    QString firstItem;

    for (const QVariantMap &line : m_prItems) {
        if (line["prNo"].toString() != prNo) continue;
        if (count == 0) firstItem = line["itemName"].toString();
        ++count;
        totalQty += line["qty"].toInt();
        estimatedValue += line["qty"].toInt() * line["estimatedPrice"].toDouble();
    }

    pr["itemCount"]      = count;
    pr["totalQty"]       = totalQty;
    pr["estimatedValue"] = estimatedValue;
    pr["summary"]        = (count <= 1)
                               ? firstItem
                               : firstItem + " +" + QString::number(count - 1) + " more";
}

// Replaces this request's lines with the supplied ones. Serial ids are left to
// the database, so the rows are deleted and re-inserted rather than patched.
void PurchaseRequestService::saveItemsFor(const QString &prNo, const QVariantList &items)
{
    if (!m_db) return;
    m_db->removeRow("pr_items", "pr_no", prNo);

    for (const QVariant &v : items) {
        QVariantMap line = v.toMap();
        line["prNo"] = prNo;
        QVariantMap dbRow = appRowToDb(kPrItemFields, line);
        dbRow.remove("id");
        m_db->insert("pr_items", dbRow);
    }
    m_prItems = dbRowsToApp(kPrItemFields, m_db->selectAll("pr_items", "id"));
}

QString PurchaseRequestService::nextNumber()
{
    int next = m_counters->peek("pr");
    return "PR-" + QString::number(next).rightJustified(4, '0');
}

// Validates the lines of a request, returning them normalised. Blanks are
// filled in from the Item Master so asking for a catalogued part means typing
// its name and how many, and nothing else.
static bool resolvePRLines(const QVariantList &items,
                           const QVector<QVariantMap> &itemMaster,
                           QVariantList *out, QString *error)
{
    if (items.isEmpty()) {
        *error = QStringLiteral("Add at least one item to the purchase request");
        return false;
    }

    for (const QVariant &v : items) {
        QVariantMap line = v.toMap();
        const QString itemName = line["itemName"].toString().trimmed();
        if (itemName.isEmpty()) {
            *error = QStringLiteral("Item Name is required for every requested line");
            return false;
        }
        if (line["qty"].toInt() <= 0) {
            *error = QStringLiteral("Quantity must be greater than 0 for: ") + itemName;
            return false;
        }

        for (const QVariantMap &item : itemMaster) {
            if (item["partName"].toString().trimmed().compare(itemName, Qt::CaseInsensitive) != 0)
                continue;
            if (line["partNo"].toString().trimmed().isEmpty())
                line["partNo"] = item["partNo"].toString().trimmed();
            if (line["unit"].toString().trimmed().isEmpty())
                line["unit"] = item["unit"].toString().trimmed();
            if (line["vendor"].toString().trimmed().isEmpty())
                line["vendor"] = item["vendor"].toString().trimmed();
            if (line["estimatedPrice"].toDouble() <= 0.0)
                line["estimatedPrice"] = item["unitPrice"].toDouble();
            break;
        }

        line["itemName"] = itemName;
        out->append(line);
    }
    return true;
}

// Copies the request fields QML supplies onto a header row, leaving anything it
// did not send untouched (which is what makes this usable for both create and
// edit).
static void applyPRFields(QVariantMap &pr, const QVariantMap &request)
{
    static const QStringList kEditable = {
        "date", "requestedBy", "department", "neededBy", "priority", "remarks"
    };
    for (const QString &key : kEditable) {
        if (request.contains(key))
            pr[key] = request.value(key).toString().trimmed();
    }
}

QString PurchaseRequestService::create(const QVariantMap &request,
                                            const QVariantList &items)
{
    // Validate every line BEFORE anything is written.
    QVariantList lines;
    QString error;
    if (!resolvePRLines(items, m_itemMaster->rows(), &lines, &error)) {
        emit errorOccurred(error);
        return "";
    }

    // Atomic, shared request number so concurrent users never collide.
    int prSeq = m_counters->next("pr");
    const QString prNo = "PR-" + QString::number(prSeq).rightJustified(4, '0');

    QVariantMap pr;
    pr["prNo"]       = prNo;
    pr["status"]     = "Pending";
    pr["date"]       = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    pr["priority"]   = "Normal";
    pr["reviewedBy"] = "";
    pr["reviewNote"] = "";
    pr["poNo"]       = "";
    applyPRFields(pr, request);
    if (pr["date"].toString().trimmed().isEmpty())
        pr["date"] = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    if (pr["requestedBy"].toString().trimmed().isEmpty())
        pr["requestedBy"] = m_session->currentUser();
    if (pr["priority"].toString().trimmed().isEmpty())
        pr["priority"] = "Normal";

    saveItemsFor(prNo, lines);
    recalcHeader(pr);
    m_requests.append(pr);
    save();

    emit listChanged();
    emit created(prNo);

    qDebug() << "Created purchase request:" << prNo << "with" << lines.size() << "items";
    return prNo;
}

bool PurchaseRequestService::update(const QString &prNo,
                                         const QVariantMap &request,
                                         const QVariantList &items)
{
    int index = -1;
    for (int i = 0; i < m_requests.size(); ++i) {
        if (m_requests[i]["prNo"].toString() == prNo) { index = i; break; }
    }
    if (index == -1) {
        emit errorOccurred("Purchase request not found: " + prNo);
        return false;
    }
    if (m_requests[index]["status"].toString().compare("Pending", Qt::CaseInsensitive) != 0) {
        emit errorOccurred("Only a pending request can be edited. " + prNo +
                           " has already been " +
                           m_requests[index]["status"].toString().toLower() + ".");
        return false;
    }

    QVariantList lines;
    QString error;
    if (!resolvePRLines(items, m_itemMaster->rows(), &lines, &error)) {
        emit errorOccurred(error);
        return false;
    }

    QVariantMap pr = m_requests[index];
    applyPRFields(pr, request);

    saveItemsFor(prNo, lines);
    recalcHeader(pr);
    m_requests[index] = pr;
    save();

    emit listChanged();
    qDebug() << "Updated purchase request:" << prNo;
    return true;
}

bool PurchaseRequestService::remove(const QString &prNo)
{
    for (int i = 0; i < m_requests.size(); ++i) {
        if (m_requests[i]["prNo"].toString() != prNo) continue;

        // A request that became an order is the paper trail behind that order,
        // so it stays; cancel it by rejecting it instead.
        const QString poNo = m_requests[i]["poNo"].toString().trimmed();
        if (!poNo.isEmpty()) {
            emit errorOccurred(prNo + " was ordered as " + poNo +
                               " and cannot be deleted.");
            return false;
        }

        m_requests.removeAt(i);
        save();
        if (m_db) {
            m_db->removeRow("pr_items", "pr_no", prNo);
            m_prItems = dbRowsToApp(kPrItemFields, m_db->selectAll("pr_items", "id"));
        }
        emit listChanged();
        qDebug() << "Deleted purchase request:" << prNo;
        return true;
    }
    emit errorOccurred("Purchase request not found: " + prNo);
    return false;
}

bool PurchaseRequestService::setStatus(const QString &prNo,
                                            const QString &newStatus,
                                            const QString &reviewedBy,
                                            const QString &note)
{
    for (auto &pr : m_requests) {
        if (pr["prNo"].toString() != prNo) continue;

        pr["status"] = newStatus;
        pr["reviewedBy"] = reviewedBy.trimmed().isEmpty() ? m_session->currentUser() : reviewedBy.trimmed();
        if (!note.trimmed().isEmpty())
            pr["reviewNote"] = note.trimmed();
        save();
        emit listChanged();
        qDebug() << "Request" << prNo << "status ->" << newStatus;
        return true;
    }
    emit errorOccurred("Purchase request not found: " + prNo);
    return false;
}

bool PurchaseRequestService::linkToPO(const QString &prNo, const QString &poNo)
{
    for (auto &pr : m_requests) {
        if (pr["prNo"].toString() != prNo) continue;

        pr["poNo"]   = poNo;
        pr["status"] = "Ordered";
        save();
        emit listChanged();
        qDebug() << "Request" << prNo << "ordered as" << poNo;
        return true;
    }
    emit errorOccurred("Purchase request not found: " + prNo);
    return false;
}

QVariantList PurchaseRequestService::list(const QString &statusFilter)
{
    QVariantList list;
    // Newest first: the queue is worked from the top, and what was just asked
    // for is what someone is waiting on.
    for (int i = m_requests.size() - 1; i >= 0; --i) {
        const QVariantMap &pr = m_requests[i];
        if (statusFilter.isEmpty() ||
            pr["status"].toString().toLower() == statusFilter.toLower()) {
            list.append(pr);
        }
    }
    return list;
}

QVariantMap PurchaseRequestService::searchIndex() const
{
    QHash<QString, QStringList> lineText;
    for (const auto &line : m_prItems) {
        lineText[line["prNo"].toString()]
                << line["itemName"].toString() << line["partNo"].toString()
                << line["vendor"].toString() << line["unit"].toString();
    }

    QVariantMap index;
    for (const auto &pr : m_requests) {
        const QString prNo = pr["prNo"].toString();
        QStringList fields{prNo,
                           pr["date"].toString(),
                           pr["requestedBy"].toString(),
                           pr["department"].toString(),
                           pr["neededBy"].toString(),
                           pr["priority"].toString(),
                           pr["status"].toString(),
                           pr["remarks"].toString(),
                           pr["reviewedBy"].toString(),
                           pr["reviewNote"].toString(),
                           pr["poNo"].toString()};
        fields += lineText.value(prNo);
        index[prNo] = fields.join(QLatin1Char(' ')).toLower();
    }
    return index;
}

QVariantList PurchaseRequestService::itemsFor(const QString &prNo)
{
    QVariantList list;
    for (const auto &line : m_prItems) {
        if (line["prNo"].toString() == prNo)
            list.append(line);
    }
    return list;
}

QVariantMap PurchaseRequestService::byNumber(const QString &prNo)
{
    for (const auto &pr : m_requests) {
        if (pr["prNo"].toString() == prNo) return pr;
    }
    return QVariantMap();
}

int PurchaseRequestService::pendingCount() const
{
    int count = 0;
    for (const auto &pr : m_requests) {
        if (pr["status"].toString().compare("Pending", Qt::CaseInsensitive) == 0)
            ++count;
    }
    return count;
}
