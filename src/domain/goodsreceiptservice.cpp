#include "domain/goodsreceiptservice.h"

#include "core/appsettings.h"
#include "core/dbmanager.h"
#include "core/dbschema.h"
#include "domain/purchaseorderservice.h"
#include "domain/stockmovementservice.h"
#include "domain/stockservice.h"
#include "models/exceltablemodel.h"

#include <QDateTime>
#include <QDebug>

// Stock cells can hold a quantity typed as text, so every read goes here.
static int toStockInt(const QVariant &value)
{
    bool ok = false;
    double number = value.toDouble(&ok);
    if (!ok) {
        number = value.toString().trimmed().toDouble(&ok);
        if (!ok) return 0;
    }
    return qRound(number);
}

void GoodsReceiptService::load()
{
    m_records.clear();
    if (!m_db) return;
    m_records = dbRowsToApp(kGrnFields, m_db->selectAll("grn_records", "grn_no"));
    qDebug() << "Loaded" << m_records.size() << "GRN records";
}

void GoodsReceiptService::save()
{
    if (!m_db) return;
    m_db->replaceAll("grn_records", appRowsToDb(kGrnFields, m_records));
}

QString GoodsReceiptService::receiveForItem(int itemId,
                                          int receivedQty, int acceptedQty,
                                          int rejectedQty, const QString &remarks,
                                          const QString &receivedBy)
{
    // Find the PO line item.
    QVariantMap *line = nullptr;
    for (auto &item : m_po->lineItems()) {
        if (item["id"].toInt() == itemId) {
            line = &item;
            break;
        }
    }
    if (!line) {
        emit errorOccurred("PO item not found (id " + QString::number(itemId) + ")");
        return "";
    }

    const QString poNo = line->value("poNo").toString();

    // Find the PO header.
    QVariantMap *targetPO = nullptr;
    for (auto &po : m_po->orders()) {
        if (po["poNo"].toString() == poNo) {
            targetPO = &po;
            break;
        }
    }
    if (!targetPO) {
        emit errorOccurred("Purchase Order not found: " + poNo);
        return "";
    }

    QString status = targetPO->value("status").toString().toLower();
    if (status == "closed" || status == "cancelled") {
        emit errorOccurred("Cannot receive against a " + status + " PO");
        return "";
    }

    if (receivedQty <= 0) {
        emit errorOccurred("Received quantity must be greater than 0");
        return "";
    }

    QString receiverName = receivedBy.trimmed().isEmpty() ? m_session->currentUser() : receivedBy.trimmed();

    // Generate GRN number (atomic, shared)
    int grnSeq = m_counters->next("grn");
    QString grnNo = "GRN-" + QString::number(grnSeq).rightJustified(4, '0');

    const QString partName = line->value("partName").toString();
    const QString partNo = line->value("partNo").toString();

    // Create GRN record for this line.
    QVariantMap grn;
    grn["grnNo"]       = grnNo;
    grn["poNo"]        = poNo;
    grn["date"]        = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    grn["partName"]    = partName;
    grn["receivedQty"] = receivedQty;
    grn["acceptedQty"] = acceptedQty;
    grn["rejectedQty"] = rejectedQty;
    grn["remarks"]     = remarks;
    grn["receivedBy"]  = receiverName;

    m_records.append(grn);
    save();

    // Update the line's received quantity (in memory and in the database).
    (*line)["receivedQty"] = line->value("receivedQty").toInt() + receivedQty;
    if (m_db) {
        QVariantMap upd;
        upd["id"] = itemId;
        upd["received_qty"] = line->value("receivedQty").toInt();
        m_db->upsert("po_items", {"id"}, upd);
    }

    // Recompute the header from all lines: Received only when every line is
    // fully received, otherwise Partially Received.
    m_po->recalcHeader(*targetPO);
    bool allReceived = true;
    for (const auto &item : m_po->lineItems()) {
        if (item["poNo"].toString() != poNo) continue;
        if (item["receivedQty"].toInt() < item["qty"].toInt()) {
            allReceived = false;
            break;
        }
    }
    (*targetPO)["status"] = allReceived ? "Received" : "Partially Received";
    (*targetPO)["receivedBy"] = receiverName;
    (*targetPO)["receivedDate"] = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    m_po->save();
    
    // Update stock in main table (add accepted quantity). Stock column = 2.
    int partRow = m_stock->findRowByName(partName);
    QString lineDepartment = line->value("department").toString().trimmed();
    QString poPreparedBy = targetPO->value("preparedBy").toString().trimmed();
    QString poApprovedBy = targetPO->value("approvedBy").toString().trimmed();
    QString lineVendor = line->value("vendor").toString().trimmed();
    double lineUnitPrice = line->value("unitPrice").toDouble();
    QString receivedDate = (*targetPO)["receivedDate"].toString();

    if (partRow != -1) {
        int currentStock = toStockInt(m_model->getData(partRow, 2));
        m_model->setDataAt(partRow, 2, currentStock + acceptedQty);

        // 3=Department, 4=Prepared, 5=Approved, 6=Vendor, 7=Date, 8=Unit Price
        if (!lineDepartment.isEmpty()) m_model->setDataAt(partRow, 3, lineDepartment);
        if (!poPreparedBy.isEmpty()) m_model->setDataAt(partRow, 4, poPreparedBy);
        if (!poApprovedBy.isEmpty()) m_model->setDataAt(partRow, 5, poApprovedBy);
        if (!lineVendor.isEmpty()) m_model->setDataAt(partRow, 6, lineVendor);
        if (!receivedDate.isEmpty()) m_model->setDataAt(partRow, 7, receivedDate);
        if (lineUnitPrice > 0.0) m_model->setDataAt(partRow, 8, lineUnitPrice);

        qDebug() << "Stock updated:" << partName << currentStock << "->" << (currentStock + acceptedQty);
    } else {
        // Part not found in stock table - add new row
        m_model->addRow();
        int newRow = m_model->rowCount() - 1;
        m_model->setDataAt(newRow, 0, partName);
        m_model->setDataAt(newRow, 1, partNo);
        m_model->setDataAt(newRow, 2, acceptedQty);
        m_model->setDataAt(newRow, 3, lineDepartment);
        m_model->setDataAt(newRow, 4, poPreparedBy);
        m_model->setDataAt(newRow, 5, poApprovedBy);
        m_model->setDataAt(newRow, 6, lineVendor);
        m_model->setDataAt(newRow, 7, receivedDate);
        if (lineUnitPrice > 0.0) m_model->setDataAt(newRow, 8, lineUnitPrice);

        qDebug() << "New part added to stock:" << partName << "qty:" << acceptedQty;
    }

    // Log stock movement
    m_movements->log(partName, partNo, "IN", acceptedQty, grnNo + " (from " + poNo + ")", receiverName);

    if (rejectedQty > 0) {
        m_movements->log(partName, partNo, "REJECTED", rejectedQty, grnNo, receiverName);
    }

    // Persist the updated stock (database is the permanent store).
    m_stock->persist();

    emit received(grnNo, poNo);

    qDebug() << "GRN" << grnNo << "for PO" << poNo << "item" << partName
             << "| Accepted:" << acceptedQty << "Rejected:" << rejectedQty;

    return grnNo;
}

QString GoodsReceiptService::receiveForOrder(const QString &poNo,
                                   int receivedQty, int acceptedQty,
                                   int rejectedQty, const QString &remarks,
                                   const QString &receivedBy)
{
    // Legacy entry point: receive against the first line that is still open.
    int itemId = -1;
    for (const auto &line : m_po->lineItems()) {
        if (line["poNo"].toString() != poNo) continue;
        if (itemId == -1) itemId = line["id"].toInt();
        if (line["receivedQty"].toInt() < line["qty"].toInt()) {
            itemId = line["id"].toInt();
            break;
        }
    }
    if (itemId == -1) {
        emit errorOccurred("No items found for PO: " + poNo);
        return "";
    }
    return receiveForItem(itemId, receivedQty, acceptedQty, rejectedQty, remarks, receivedBy);
}

QVariantList GoodsReceiptService::list()
{
    QVariantList list;
    for (const auto &grn : m_records) {
        list.append(grn);
    }
    return list;
}

