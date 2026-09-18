#include "domain/purchaseorderservice.h"

#include "core/appsettings.h"
#include "core/dbmanager.h"
#include "core/dbschema.h"
#include "documents/podocument.h"
#include "domain/itemmasterservice.h"
#include "domain/stockmovementservice.h"
#include "domain/stockservice.h"
#include "domain/vendorservice.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

void PurchaseOrderService::load()
{
    m_orders.clear();
    if (!m_db) return;
    m_orders = dbRowsToApp(kPoFields, m_db->selectAll("purchase_orders", "po_no"));

    // Apply the same defaults the old loader used.
    for (QVariantMap &po : m_orders) {
        if (po["status"].toString().trimmed().isEmpty())
            po["status"] = "Draft";
        if (po["totalAmount"].toDouble() <= 0.0 && po["qty"].toInt() > 0 && po["unitPrice"].toDouble() > 0.0)
            po["totalAmount"] = po["qty"].toInt() * po["unitPrice"].toDouble();
    }

    loadItems();

    qDebug() << "Loaded" << m_orders.size() << "purchase orders ("
             << m_poItems.size() << "line items )";
}

void PurchaseOrderService::save()
{
    if (!m_db) return;
    m_db->replaceAll("purchase_orders", appRowsToDb(kPoFields, m_orders));
}

void PurchaseOrderService::loadItems()
{
    m_poItems.clear();
    if (!m_db) return;
    m_poItems = dbRowsToApp(kPoItemFields, m_db->selectAll("po_items", "id"));

    // Keep header aggregates (qty/total/received/item count) in sync with
    // the lines so the PO list always reflects the shared database.
    for (QVariantMap &po : m_orders)
        recalcHeader(po);
}

void PurchaseOrderService::recalcHeader(QVariantMap &po)
{
    const QString poNo = po["poNo"].toString();
    int count = 0, totalQty = 0, totalReceived = 0;
    double totalAmount = 0.0;
    QString firstPart, firstPartNo, firstDept;
    QStringList vendors;

    for (const QVariantMap &line : m_poItems) {
        if (line["poNo"].toString() != poNo) continue;
        if (count == 0) {
            firstPart = line["partName"].toString();
            firstPartNo = line["partNo"].toString();
            firstDept = line["department"].toString();
        }
        ++count;
        totalQty += line["qty"].toInt();
        totalReceived += line["receivedQty"].toInt();
        totalAmount += line["totalAmount"].toDouble();
        const QString vendor = line["vendor"].toString().trimmed();
        if (!vendor.isEmpty() && !vendors.contains(vendor))
            vendors << vendor;
    }

    if (count == 0) return;   // header-only PO (should not happen post-migration)

    po["itemCount"] = count;
    po["qty"] = totalQty;
    po["receivedQty"] = totalReceived;
    po["totalAmount"] = totalAmount;
    po["partName"] = (count == 1) ? firstPart
                                  : firstPart + " +" + QString::number(count - 1) + " more";
    po["partNo"] = (count == 1) ? firstPartNo : QString();
    po["department"] = (count == 1) ? firstDept : QString();
    po["vendor"] = vendors.join(", ");
    if (count > 1)
        po["unitPrice"] = 0.0;   // meaningless across mixed lines
}

bool PurchaseOrderService::resolveLine(QVariantMap &line)
{
    const QString partName = line["partName"].toString().trimmed();
    if (partName.isEmpty()) {
        emit errorOccurred("Part Name is required for every PO item");
        return false;
    }
    if (line["qty"].toInt() <= 0) {
        emit errorOccurred("Quantity must be greater than 0 for: " + partName);
        return false;
    }

    // Fill blanks from the item master (matching the old single-item logic).
    for (const auto &item : m_itemMaster->rows()) {
        if (item["partName"].toString().trimmed().compare(partName, Qt::CaseInsensitive) == 0) {
            if (line["partNo"].toString().trimmed().isEmpty())
                line["partNo"] = item["partNo"].toString().trimmed();
            if (line["department"].toString().trimmed().isEmpty())
                line["department"] = item["department"].toString().trimmed();
            if (line["vendor"].toString().trimmed().isEmpty())
                line["vendor"] = item["vendor"].toString().trimmed();
            if (line["unitPrice"].toDouble() <= 0.0)
                line["unitPrice"] = item["unitPrice"].toDouble();
            break;
        }
    }

    if (line["vendor"].toString().trimmed().isEmpty()) {
        emit errorOccurred("Vendor is required for: " + partName);
        return false;
    }

    line["partName"] = partName;
    line["totalAmount"] = line["qty"].toInt() * line["unitPrice"].toDouble();
    line["receivedQty"] = 0;
    return true;
}

QString PurchaseOrderService::buildHtml(const QString &poNo, const QString &comments) const
{
    QVariantMap po;
    for (const auto &row : m_orders) {
        if (row["poNo"].toString() == poNo) { po = row; break; }
    }
    if (po.isEmpty())
        return QString();

    QVariantList items;
    QSet<QString> vendorNames;
    for (const auto &line : m_poItems) {
        if (line["poNo"].toString() != poNo) continue;
        items.append(line);
        const QString v = line["vendor"].toString().trimmed();
        if (!v.isEmpty()) vendorNames.insert(v);
    }

    // A PO raised against a single vendor gets that vendor's full address
    // block; a mixed-vendor order can only sensibly list the names.
    QVariantMap vendor;
    if (vendorNames.size() == 1) {
        const QString only = *vendorNames.constBegin();
        vendor = m_vendorService->byName(only);
        if (vendor.isEmpty()) vendor["vendorName"] = only;
    } else if (!vendorNames.isEmpty()) {
        QStringList names(vendorNames.constBegin(), vendorNames.constEnd());
        names.sort();
        vendor["vendorName"] = names.join(", ");
    }

    return PoDocument::buildHtml(loadCompanyProfile(), vendor, po, items, comments);
}

QString PurchaseOrderService::defaultPdfPath(const QString &poNo) const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return base + "/Enstein Stock Manager/Purchase Orders/" + poNo + ".pdf";
}

QVariantMap PurchaseOrderService::generatePreview(const QString &poNo, const QString &comments)
{
    const QString html = buildHtml(poNo, comments);
    if (html.isEmpty()) {
        emit errorOccurred("Purchase order not found: " + poNo);
        return QVariantMap();
    }

    // A per-PO scratch folder, wiped first so a re-preview never shows pages
    // left over from an earlier render.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/EnsteinStockManager/po-preview/" + poNo;
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);

    const QString pdfPath = dir + "/" + poNo + ".pdf";
    if (!PoDocument::writePdf(html, pdfPath)) {
        emit errorOccurred("Could not render the purchase order PDF");
        return QVariantMap();
    }

    QStringList pageUrls;
    for (const QString &f : PoDocument::renderPages(html, dir))
        pageUrls << QUrl::fromLocalFile(f).toString();

    QVariantMap result;
    result["poNo"]    = poNo;
    result["pdfPath"] = pdfPath;
    result["pages"]   = pageUrls;
    return result;
}

QString PurchaseOrderService::savePdf(const QString &poNo, const QString &destPath,
                                const QString &comments)
{
    const QString html = buildHtml(poNo, comments);
    if (html.isEmpty()) {
        emit errorOccurred("Purchase order not found: " + poNo);
        return QString();
    }

    QString target = destPath.trimmed();
    if (target.startsWith("file://"))
        target = QUrl(target).toLocalFile();
    if (target.isEmpty())
        target = defaultPdfPath(poNo);
    if (!target.endsWith(".pdf", Qt::CaseInsensitive))
        target += ".pdf";

    if (!PoDocument::writePdf(html, target)) {
        emit errorOccurred("Could not save the purchase order to " + target);
        return QString();
    }
    return target;
}

QString PurchaseOrderService::nextNumber()
{
    int next = m_counters->peek("po");
    return "PO-" + QString::number(next).rightJustified(4, '0');
}

QString PurchaseOrderService::createWithItems(const QVariantList &items,
                                               const QString &expectedDate,
                                               const QString &expectedEndDate,
                                               const QString &preparedBy)
{
    if (items.isEmpty()) {
        emit errorOccurred("Add at least one item to the purchase order");
        return "";
    }

    // Validate and resolve every line BEFORE anything is written.
    QVector<QVariantMap> lines;
    for (const QVariant &v : items) {
        QVariantMap line = v.toMap();
        if (!resolveLine(line))
            return "";
        lines.append(line);
    }

    const QString resolvedPreparedBy =
        preparedBy.trimmed().isEmpty() ? m_session->currentUser() : preparedBy.trimmed();

    // Atomic, shared PO number so concurrent users never collide.
    int poSeq = m_counters->next("po");
    QString poNo = "PO-" + QString::number(poSeq).rightJustified(4, '0');

    // Header (aggregates recomputed from lines by recalcPOHeader).
    QVariantMap po;
    po["poNo"]         = poNo;
    po["date"]         = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    po["vendor"]       = "";
    po["partName"]     = "";
    po["partNo"]       = "";
    po["department"]   = "";
    po["qty"]          = 0;
    po["unitPrice"]    = 0.0;
    po["totalAmount"]  = 0.0;
    po["expectedDate"] = expectedDate;
    po["expectedEndDate"] = expectedEndDate;
    po["status"]       = "Draft";
    po["receivedQty"]  = 0;
    po["preparedBy"]   = resolvedPreparedBy;
    po["approvedBy"]   = "";
    po["receivedBy"]   = "";
    po["receivedDate"] = "";

    // Persist lines first (insert without id so the serial is assigned),
    // then reload to pick up the generated ids.
    for (QVariantMap &line : lines) {
        line["poNo"] = poNo;
        QVariantMap dbRow = appRowToDb(kPoItemFields, line);
        dbRow.remove("id");
        if (m_db) m_db->insert("po_items", dbRow);
    }
    if (m_db) m_poItems = dbRowsToApp(kPoItemFields, m_db->selectAll("po_items", "id"));

    recalcHeader(po);
    m_orders.append(po);
    save();
    emit listChanged();
    emit created(poNo);

    for (const QVariantMap &line : lines) {
        m_movements->log(line["partName"].toString(), line["partNo"].toString(),
                         "PO_CREATED", line["qty"].toInt(), poNo, m_session->currentUser());
    }

    qDebug() << "Created PO:" << poNo << "with" << lines.size() << "items";
    return poNo;
}

QVariantList PurchaseOrderService::itemsFor(const QString &poNo)
{
    QVariantList list;
    for (const auto &line : m_poItems) {
        if (line["poNo"].toString() == poNo)
            list.append(line);
    }
    return list;
}

QString PurchaseOrderService::create(const QString &vendor,
                                          const QString &partName,
                                          const QString &partNo,
                                          int qty, double unitPrice,
                                          const QString &expectedDate,
                                          const QString &department,
                                          const QString &preparedBy,
                                          const QString &expectedEndDate)
{
    QVariantMap line;
    line["partName"]   = partName;
    line["partNo"]     = partNo;
    line["vendor"]     = vendor;
    line["department"] = department;
    line["qty"]        = qty;
    line["unitPrice"]  = unitPrice;
    return createWithItems({line}, expectedDate, expectedEndDate, preparedBy);
}

bool PurchaseOrderService::sendForApproval(const QString &poNo, const QString &approvedBy)
{
    QString approver = approvedBy.trimmed();
    if (approver.isEmpty()) {
        emit errorOccurred("Approved By name is required");
        return false;
    }

    for (auto &po : m_orders) {
        if (po["poNo"].toString() == poNo) {
            QString status = po["status"].toString().toLower();
            if (status != "draft") {
                emit errorOccurred("Only Draft PO can be sent");
                return false;
            }

            po["status"] = "Sent";
            po["approvedBy"] = approver;
            save();
            emit listChanged();
            return true;
        }
    }

    emit errorOccurred("PO not found: " + poNo);
    return false;
}

QVariantList PurchaseOrderService::list(const QString &statusFilter)
{
    QVariantList list;
    for (const auto &po : m_orders) {
        if (statusFilter.isEmpty() ||
            po["status"].toString().toLower() == statusFilter.toLower()) {
            list.append(po);
        }
    }
    return list;
}

QVariantMap PurchaseOrderService::searchIndex() const
{
    // Group the line items by PO first so each order is visited once.
    QHash<QString, QStringList> lineText;
    for (const auto &line : m_poItems) {
        lineText[line["poNo"].toString()]
                << line["partName"].toString() << line["partNo"].toString()
                << line["vendor"].toString() << line["department"].toString();
    }

    QVariantMap index;
    for (const auto &po : m_orders) {
        const QString poNo = po["poNo"].toString();
        QStringList fields{poNo,
                           po["vendor"].toString(),
                           po["partName"].toString(),
                           po["partNo"].toString(),
                           po["department"].toString(),
                           po["date"].toString(),
                           po["expectedDate"].toString(),
                           po["expectedEndDate"].toString(),
                           po["status"].toString(),
                           po["preparedBy"].toString(),
                           po["approvedBy"].toString(),
                           po["receivedBy"].toString(),
                           po["receivedDate"].toString()};
        fields += lineText.value(poNo);
        index[poNo] = fields.join(QLatin1Char(' ')).toLower();
    }
    return index;
}

bool PurchaseOrderService::setStatus(const QString &poNo, const QString &newStatus)
{
    for (auto &po : m_orders) {
        if (po["poNo"].toString() == poNo) {
            po["status"] = newStatus;
            save();
            emit listChanged();
            qDebug() << "PO" << poNo << "status ->" << newStatus;
            return true;
        }
    }
    emit errorOccurred("PO not found: " + poNo);
    return false;
}

bool PurchaseOrderService::update(const QString &poNo, const QVariantMap &poDetails)
{
    for (auto &po : m_orders) {
        if (po["poNo"].toString() != poNo) continue;

        QString status = po["status"].toString().toLower();
        if (status == "received" || status == "closed" || status == "cancelled") {
            emit errorOccurred("Cannot edit PO in status: " + po["status"].toString());
            return false;
        }

        QString vendor = poDetails.value("vendor", po["vendor"]).toString().trimmed();
        QString partName = poDetails.value("partName", po["partName"]).toString().trimmed();
        QString partNo = poDetails.value("partNo", po["partNo"]).toString().trimmed();
        QString department = poDetails.value("department", po["department"]).toString().trimmed();
        QString expectedDate = poDetails.value("expectedDate", po["expectedDate"]).toString().trimmed();
        QString expectedEndDate = poDetails.value("expectedEndDate", po["expectedEndDate"]).toString().trimmed();
        QString preparedBy = poDetails.value("preparedBy", po["preparedBy"]).toString().trimmed();
        int qty = poDetails.value("qty", po["qty"]).toInt();
        double unitPrice = poDetails.value("unitPrice", po["unitPrice"]).toDouble();

        if (vendor.isEmpty() || partName.isEmpty()) {
            emit errorOccurred("Vendor and Part Name are required");
            return false;
        }
        if (qty <= 0) {
            emit errorOccurred("Quantity must be greater than 0");
            return false;
        }

        po["expectedDate"] = expectedDate;
        po["expectedEndDate"] = expectedEndDate;
        po["preparedBy"] = preparedBy;

        // Sync the line item when this PO has exactly one (the edit dialog
        // is only offered for single-item POs).
        QVector<QVariantMap *> poLines;
        for (auto &line : m_poItems)
            if (line["poNo"].toString() == poNo) poLines.append(&line);

        if (poLines.size() == 1) {
            QVariantMap &line = *poLines.first();
            line["vendor"] = vendor;
            line["partName"] = partName;
            line["partNo"] = partNo;
            line["department"] = department;
            line["qty"] = qty;
            line["unitPrice"] = unitPrice;
            line["totalAmount"] = qty * unitPrice;
            if (m_db) {
                QVariantMap dbRow = appRowToDb(kPoItemFields, line);
                m_db->upsert("po_items", {"id"}, dbRow);
            }
        }
        recalcHeader(po);

        save();
        emit listChanged();
        return true;
    }

    emit errorOccurred("PO not found: " + poNo);
    return false;
}

QVariantMap PurchaseOrderService::byNumber(const QString &poNo)
{
    for (const auto &po : m_orders) {
        if (po["poNo"].toString() == poNo) return po;
    }
    return QVariantMap();
}

int PurchaseOrderService::pendingCount() const
{
    int count = 0;
    for (const auto &po : m_orders) {
        QString status = po["status"].toString().toLower();
        if (status == "draft" || status == "sent" || status == "partially received") {
            count++;
        }
    }
    return count;
}
