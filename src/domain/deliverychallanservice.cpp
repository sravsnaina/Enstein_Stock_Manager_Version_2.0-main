#include "domain/deliverychallanservice.h"

#include "core/appsettings.h"
#include "core/dbmanager.h"
#include "core/dbschema.h"
#include "documents/dcdocument.h"
#include "domain/itemmasterservice.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

void DeliveryChallanService::load()
{
    m_challans.clear();
    if (!m_db) return;
    m_challans = dbRowsToApp(kDcFields, m_db->selectAll("delivery_challans", "dc_no"));

    for (QVariantMap &dc : m_challans) {
        if (dc["status"].toString().trimmed().isEmpty())
            dc["status"] = "Draft";
    }

    loadItems();

    qDebug() << "Loaded" << m_challans.size() << "delivery challans ("
             << m_dcItems.size() << "line items )";
}

void DeliveryChallanService::save()
{
    if (!m_db) return;
    m_db->replaceAll("delivery_challans", appRowsToDb(kDcFields, m_challans));
}

void DeliveryChallanService::loadItems()
{
    m_dcItems.clear();
    if (!m_db) return;
    m_dcItems = dbRowsToApp(kDcItemFields, m_db->selectAll("dc_items", "id"));

    for (QVariantMap &dc : m_challans)
        recalcHeader(dc);
}

void DeliveryChallanService::recalcHeader(QVariantMap &dc)
{
    const QString dcNo = dc["dcNo"].toString();
    int count = 0;
    double totalQty = 0.0;
    QString firstItem;

    for (const QVariantMap &line : m_dcItems) {
        if (line["dcNo"].toString() != dcNo) continue;
        if (count == 0) firstItem = line["itemName"].toString();
        ++count;
        totalQty += line["qty"].toDouble();
    }

    dc["itemCount"] = count;
    dc["totalQty"]  = totalQty;
    dc["summary"]   = (count <= 1)
                          ? firstItem
                          : firstItem + " +" + QString::number(count - 1) + " more";
}

// Replaces this challan's lines with the supplied ones. Serial ids are left to
// the database, so the rows are deleted and re-inserted rather than patched.
void DeliveryChallanService::saveItemsFor(const QString &dcNo, const QVariantList &items)
{
    if (!m_db) return;
    m_db->removeRow("dc_items", "dc_no", dcNo);

    for (const QVariant &v : items) {
        QVariantMap line = v.toMap();
        line["dcNo"] = dcNo;
        QVariantMap dbRow = appRowToDb(kDcItemFields, line);
        dbRow.remove("id");
        m_db->insert("dc_items", dbRow);
    }
    m_dcItems = dbRowsToApp(kDcItemFields, m_db->selectAll("dc_items", "id"));
}

QString DeliveryChallanService::nextNumber()
{
    int next = m_counters->peek("dc");
    return "DC-" + QString::number(next).rightJustified(4, '0');
}

// Validates the lines of a challan, returning them normalised. Blank HSN codes
// and units are filled in from the Item Master so a challan does not mean
// retyping what the catalogue already knows.
static bool resolveDCLines(const QVariantList &items,
                           const QVector<QVariantMap> &itemMaster,
                           QVariantList *out, QString *error)
{
    if (items.isEmpty()) {
        *error = QStringLiteral("Add at least one item to the delivery challan");
        return false;
    }

    for (const QVariant &v : items) {
        QVariantMap line = v.toMap();
        const QString itemName = line["itemName"].toString().trimmed();
        if (itemName.isEmpty()) {
            *error = QStringLiteral("Item Name is required for every challan line");
            return false;
        }
        if (line["qty"].toDouble() <= 0.0) {
            *error = QStringLiteral("Quantity must be greater than 0 for: ") + itemName;
            return false;
        }

        for (const QVariantMap &item : itemMaster) {
            if (item["partName"].toString().trimmed().compare(itemName, Qt::CaseInsensitive) != 0)
                continue;
            if (line["partNo"].toString().trimmed().isEmpty())
                line["partNo"] = item["partNo"].toString().trimmed();
            if (line["hsnCode"].toString().trimmed().isEmpty())
                line["hsnCode"] = itemTaxCode(item);
            if (line["unit"].toString().trimmed().isEmpty())
                line["unit"] = item["unit"].toString().trimmed();
            break;
        }

        line["itemName"] = itemName;
        out->append(line);
    }
    return true;
}

// Copies the challan fields QML supplies onto a header row, leaving anything it
// did not send untouched (which is what makes this usable for both create and
// edit).
static void applyDCFields(QVariantMap &dc, const QVariantMap &challan)
{
    static const QStringList kEditable = {
        "date", "deliveryTime", "partyName", "partyAddress", "partyPhone",
        "partyEmail", "partyGstin", "shipName", "shipAddress", "shipPhone",
        "shipEmail", "shipGstin", "terms", "preparedBy", "deliveredBy",
        "receivedBy"
    };
    for (const QString &key : kEditable) {
        if (challan.contains(key))
            dc[key] = challan.value(key).toString().trimmed();
    }
}

QString DeliveryChallanService::create(const QVariantMap &challan,
                                            const QVariantList &items)
{
    if (challan.value("partyName").toString().trimmed().isEmpty()) {
        emit errorOccurred("Party Name is required on a delivery challan");
        return "";
    }

    // Validate every line BEFORE anything is written.
    QVariantList lines;
    QString error;
    if (!resolveDCLines(items, m_itemMaster->rows(), &lines, &error)) {
        emit errorOccurred(error);
        return "";
    }

    // Atomic, shared challan number so concurrent users never collide.
    int dcSeq = m_counters->next("dc");
    const QString dcNo = "DC-" + QString::number(dcSeq).rightJustified(4, '0');

    QVariantMap dc;
    dc["dcNo"]   = dcNo;
    dc["status"] = "Draft";
    dc["date"]   = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    applyDCFields(dc, challan);
    if (dc["date"].toString().trimmed().isEmpty())
        dc["date"] = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    if (dc["preparedBy"].toString().trimmed().isEmpty())
        dc["preparedBy"] = m_session->currentUser();
    // Goods go to the party unless a different consignee was named.
    if (dc["shipName"].toString().trimmed().isEmpty()) {
        dc["shipName"]    = dc["partyName"];
        dc["shipAddress"] = dc["partyAddress"];
        dc["shipPhone"]   = dc["partyPhone"];
        dc["shipEmail"]   = dc["partyEmail"];
        dc["shipGstin"]   = dc["partyGstin"];
    }

    saveItemsFor(dcNo, lines);
    recalcHeader(dc);
    m_challans.append(dc);
    save();

    emit listChanged();
    emit created(dcNo);

    qDebug() << "Created delivery challan:" << dcNo << "with" << lines.size() << "items";
    return dcNo;
}

bool DeliveryChallanService::update(const QString &dcNo,
                                         const QVariantMap &challan,
                                         const QVariantList &items)
{
    int index = -1;
    for (int i = 0; i < m_challans.size(); ++i) {
        if (m_challans[i]["dcNo"].toString() == dcNo) { index = i; break; }
    }
    if (index == -1) {
        emit errorOccurred("Delivery challan not found: " + dcNo);
        return false;
    }
    if (m_challans[index]["status"].toString().compare("Draft", Qt::CaseInsensitive) != 0) {
        emit errorOccurred("Only a draft challan can be edited. " + dcNo +
                           " has already been marked " +
                           m_challans[index]["status"].toString() + ".");
        return false;
    }
    if (challan.contains("partyName") &&
        challan.value("partyName").toString().trimmed().isEmpty()) {
        emit errorOccurred("Party Name is required on a delivery challan");
        return false;
    }

    QVariantList lines;
    QString error;
    if (!resolveDCLines(items, m_itemMaster->rows(), &lines, &error)) {
        emit errorOccurred(error);
        return false;
    }

    QVariantMap dc = m_challans[index];
    applyDCFields(dc, challan);
    if (dc["shipName"].toString().trimmed().isEmpty()) {
        dc["shipName"]    = dc["partyName"];
        dc["shipAddress"] = dc["partyAddress"];
        dc["shipPhone"]   = dc["partyPhone"];
        dc["shipEmail"]   = dc["partyEmail"];
        dc["shipGstin"]   = dc["partyGstin"];
    }

    saveItemsFor(dcNo, lines);
    recalcHeader(dc);
    m_challans[index] = dc;
    save();

    emit listChanged();
    qDebug() << "Updated delivery challan:" << dcNo;
    return true;
}

bool DeliveryChallanService::remove(const QString &dcNo)
{
    for (int i = 0; i < m_challans.size(); ++i) {
        if (m_challans[i]["dcNo"].toString() != dcNo) continue;

        m_challans.removeAt(i);
        save();
        if (m_db) {
            m_db->removeRow("dc_items", "dc_no", dcNo);
            m_dcItems = dbRowsToApp(kDcItemFields, m_db->selectAll("dc_items", "id"));
        }
        emit listChanged();
        qDebug() << "Deleted delivery challan:" << dcNo;
        return true;
    }
    emit errorOccurred("Delivery challan not found: " + dcNo);
    return false;
}

bool DeliveryChallanService::setStatus(const QString &dcNo, const QString &newStatus)
{
    for (auto &dc : m_challans) {
        if (dc["dcNo"].toString() != dcNo) continue;
        dc["status"] = newStatus;
        save();
        emit listChanged();
        qDebug() << "Challan" << dcNo << "status ->" << newStatus;
        return true;
    }
    emit errorOccurred("Delivery challan not found: " + dcNo);
    return false;
}

QVariantList DeliveryChallanService::list(const QString &statusFilter)
{
    QVariantList list;
    // Newest first: a challan is usually raised, printed and handed over in one
    // sitting, so the one just made belongs at the top.
    for (int i = m_challans.size() - 1; i >= 0; --i) {
        const QVariantMap &dc = m_challans[i];
        if (statusFilter.isEmpty() ||
            dc["status"].toString().toLower() == statusFilter.toLower()) {
            list.append(dc);
        }
    }
    return list;
}

QVariantMap DeliveryChallanService::searchIndex() const
{
    QHash<QString, QStringList> lineText;
    for (const auto &line : m_dcItems) {
        lineText[line["dcNo"].toString()]
                << line["itemName"].toString() << line["partNo"].toString()
                << line["hsnCode"].toString() << line["unit"].toString();
    }

    QVariantMap index;
    for (const auto &dc : m_challans) {
        const QString dcNo = dc["dcNo"].toString();
        QStringList fields{dcNo,
                           dc["date"].toString(),
                           dc["deliveryTime"].toString(),
                           dc["partyName"].toString(),
                           dc["partyAddress"].toString(),
                           dc["partyGstin"].toString(),
                           dc["shipName"].toString(),
                           dc["shipAddress"].toString(),
                           dc["status"].toString(),
                           dc["preparedBy"].toString(),
                           dc["deliveredBy"].toString(),
                           dc["receivedBy"].toString()};
        fields += lineText.value(dcNo);
        index[dcNo] = fields.join(QLatin1Char(' ')).toLower();
    }
    return index;
}

QVariantList DeliveryChallanService::itemsFor(const QString &dcNo)
{
    QVariantList list;
    for (const auto &line : m_dcItems) {
        if (line["dcNo"].toString() == dcNo)
            list.append(line);
    }
    return list;
}

QVariantMap DeliveryChallanService::byNumber(const QString &dcNo)
{
    for (const auto &dc : m_challans) {
        if (dc["dcNo"].toString() == dcNo) return dc;
    }
    return QVariantMap();
}

QVariantList DeliveryChallanService::partyList() const
{
    QVariantList list;
    QSet<QString> seen;
    // Walked newest first so the most recent details for a party win.
    for (int i = m_challans.size() - 1; i >= 0; --i) {
        const QVariantMap &dc = m_challans[i];
        const QString name = dc["partyName"].toString().trimmed();
        if (name.isEmpty() || seen.contains(name.toLower())) continue;
        seen.insert(name.toLower());

        QVariantMap party;
        party["partyName"]    = name;
        party["partyAddress"] = dc["partyAddress"];
        party["partyPhone"]   = dc["partyPhone"];
        party["partyEmail"]   = dc["partyEmail"];
        party["partyGstin"]   = dc["partyGstin"];
        list.append(party);
    }
    return list;
}

// ---- Printable delivery challan ----

QString DeliveryChallanService::buildHtml(const QString &dcNo) const
{
    QVariantMap dc;
    for (const auto &row : m_challans) {
        if (row["dcNo"].toString() == dcNo) { dc = row; break; }
    }
    if (dc.isEmpty())
        return QString();

    QVariantList items;
    for (const auto &line : m_dcItems) {
        if (line["dcNo"].toString() == dcNo)
            items.append(line);
    }

    return DcDocument::buildHtml(loadCompanyProfile(), dc, items);
}

QString DeliveryChallanService::defaultPdfPath(const QString &dcNo) const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return base + "/Enstein Stock Manager/Delivery Challans/" + dcNo + ".pdf";
}

QVariantMap DeliveryChallanService::generatePreview(const QString &dcNo)
{
    const QString html = buildHtml(dcNo);
    if (html.isEmpty()) {
        emit errorOccurred("Delivery challan not found: " + dcNo);
        return QVariantMap();
    }

    // A per-challan scratch folder, wiped first so a re-preview never shows
    // pages left over from an earlier render.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/EnsteinStockManager/dc-preview/" + dcNo;
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    const QString pdfPath = dir + "/" + dcNo + ".pdf";
    if (!DcDocument::writePdf(html, pdfPath)) {
        emit errorOccurred("Could not render the delivery challan PDF");
        return QVariantMap();
    }

    QStringList pageUrls;
    for (const QString &f : DcDocument::renderPages(html, dir))
        pageUrls << QUrl::fromLocalFile(f).toString();

    QVariantMap result;
    result["dcNo"]    = dcNo;
    result["pdfPath"] = pdfPath;
    result["pages"]   = pageUrls;
    return result;
}

QString DeliveryChallanService::savePdf(const QString &dcNo, const QString &destPath)
{
    const QString html = buildHtml(dcNo);
    if (html.isEmpty()) {
        emit errorOccurred("Delivery challan not found: " + dcNo);
        return QString();
    }

    QString target = destPath.trimmed();
    if (target.startsWith("file://"))
        target = QUrl(target).toLocalFile();
    if (target.isEmpty())
        target = defaultPdfPath(dcNo);
    if (!target.endsWith(".pdf", Qt::CaseInsensitive))
        target += ".pdf";

    if (!DcDocument::writePdf(html, target)) {
        emit errorOccurred("Could not save the delivery challan to " + target);
        return QString();
    }
    return target;
}
