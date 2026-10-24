#include "domain/lowstockservice.h"

#include "core/appsettings.h"
#include "domain/itemmasterservice.h"
#include "domain/purchaseorderservice.h"
#include "domain/stockservice.h"
#include "domain/vendorservice.h"
#include "models/exceltablemodel.h"

#include <QDateTime>
#include <QVariant>

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

QVariantList LowStockService::items()
{
    QVariantList list;

    // Quantity already on order per part (open POs only) so the same
    // shortage is not ordered twice.
    QSet<QString> openPOs;
    for (const auto &po : m_po->orders()) {
        const QString st = po["status"].toString().toLower();
        if (st == "draft" || st == "sent" || st == "partially received")
            openPOs.insert(po["poNo"].toString());
    }
    QHash<QString, int> onOrder;
    for (const auto &line : m_po->lineItems()) {
        if (!openPOs.contains(line["poNo"].toString())) continue;
        int remaining = line["qty"].toInt() - line["receivedQty"].toInt();
        if (remaining > 0)
            onOrder[line["partName"].toString().trimmed().toLower()] += remaining;
    }

    // An item is low when current stock plus open orders cannot cover the
    // required quantity defined in the Item Master.  A completely empty shelf
    // is always an alert, even for legacy rows where no reorder quantity has
    // been configured yet.
    for (const auto &item : m_itemMaster->rows()) {
        const int required = item["requiredQty"].toInt();
        const QString partName = item["partName"].toString().trimmed();
        if (partName.isEmpty()) continue;

        int stock = 0;
        QString stockVendor;
        int row = m_stock->findRowByName(partName);
        if (row == -1) {
            // Part numbers are the stable link when an operator has used a
            // slightly different display name in the stock grid.
            const QString partNo = item["partNo"].toString().trimmed();
            if (!partNo.isEmpty()) {
                for (int candidate = 1; candidate < m_model->rowCount(); ++candidate) {
                    if (m_model->getData(candidate, 1).toString().trimmed()
                            .compare(partNo, Qt::CaseInsensitive) == 0) {
                        row = candidate;
                        break;
                    }
                }
            }
        }
        if (row != -1) {
            stock = toStockInt(m_model->getData(row, 2));
            // Older stock rows may already name a supplier even though the
            // corresponding Item Master entry predates the preferred-vendor
            // field.  Use that known supplier as a fallback for the PO.
            stockVendor = m_model->getData(row, 6).toString().trimmed();
        }

        const int ordered = onOrder.value(partName.toLower(), 0);
        // Incoming stock can satisfy a configured reorder level, but it must
        // not hide an item whose on-hand quantity has already reached zero.
        if (stock > 0 && stock + ordered >= required) continue;

        QVariantMap e;
        e["partName"]    = partName;
        e["partNo"]      = item["partNo"];
        e["stock"]       = stock;
        e["requiredQty"] = required;
        e["onOrder"]     = ordered;
        int shortage = qMax(0, required - stock - ordered);
        // Legacy/stock-only items can have no reorder quantity configured.
        // Since an empty shelf still raises an alert, give it a one-unit
        // starter order when there is no incoming stock to cover it.
        if (stock == 0 && required == 0 && ordered == 0)
            shortage = 1;
        e["shortage"]    = shortage;
        const QString preferredVendor = item["vendor"].toString().trimmed();
        QString vendor = preferredVendor.isEmpty() ? stockVendor : preferredVendor;
        // Vendor Master can classify suppliers by department/item category.
        // Use that mapping only when it identifies exactly one supplier; an
        // ambiguous match must never silently choose the wrong vendor.
        if (vendor.isEmpty() && m_vendors) {
            const QString department = item.value("department", item.value("category"))
                                          .toString().trimmed();
            QStringList matches;
            for (const QVariant &vendorValue : m_vendors->list()) {
                const QVariantMap candidate = vendorValue.toMap();
                const QString vendorDepartment = candidate.value("department").toString().trimmed();
                const QString itemCategory = candidate.value("itemCategory").toString().trimmed();
                if (!department.isEmpty()
                    && (vendorDepartment.compare(department, Qt::CaseInsensitive) == 0
                        || itemCategory.compare(department, Qt::CaseInsensitive) == 0)) {
                    const QString name = candidate.value("vendorName").toString().trimmed();
                    if (!name.isEmpty() && !matches.contains(name, Qt::CaseInsensitive))
                        matches.append(name);
                }
            }
            if (matches.size() == 1) vendor = matches.constFirst();
        }
        // A one-vendor installation has an unambiguous default.  This helps
        // legacy stock and item rows that were entered before vendor linking
        // was available, without guessing when several suppliers exist.
        if (vendor.isEmpty() && m_vendors) {
            const QStringList names = m_vendors->names();
            if (names.size() == 1) vendor = names.constFirst();
        }
        e["vendor"]      = vendor;
        e["unitPrice"]   = item["unitPrice"].toDouble();
        list.append(e);
    }
    return list;
}

int LowStockService::count()
{
    return items().size();
}

bool LowStockService::autoGeneratePO(const QVariantList &quantities)
{
    const QVariantList low = items();
    if (low.isEmpty()) {
        emit errorOccurred("No low stock items to order");
        return false;
    }

    // One multi-item PO covering every shortage; each line uses the part's
    // preferred vendor from the Item Master.
    QVariantList items;
    QStringList missingVendor;
    for (const QVariant &v : low) {
        const QVariantMap e = v.toMap();
        // A zero-stock legacy item with no reorder level is deliberately
        // displayed as an alert, but cannot safely be turned into a PO until
        // its required quantity is configured in Item Master.
        if (e["shortage"].toInt() <= 0) {
            continue;
        }
        if (e["vendor"].toString().trimmed().isEmpty()) {
            missingVendor << e["partName"].toString();
            continue;
        }
        int orderQty = e["shortage"].toInt();
        for (const QVariant &quantityValue : quantities) {
            const QVariantMap requested = quantityValue.toMap();
            const QString requestedPartNo = requested.value("partNo").toString().trimmed();
            const bool matches = !requestedPartNo.isEmpty()
                ? requestedPartNo.compare(e["partNo"].toString().trimmed(), Qt::CaseInsensitive) == 0
                : requested.value("partName").toString().trimmed().compare(
                      e["partName"].toString().trimmed(), Qt::CaseInsensitive) == 0;
            if (matches) {
                const int requestedQty = requested.value("qty").toInt();
                if (requestedQty > 0) orderQty = requestedQty;
                break;
            }
        }
        if (orderQty <= 0) continue;

        QVariantMap line;
        line["partName"]  = e["partName"];
        line["partNo"]    = e["partNo"];
        line["vendor"]    = e["vendor"];
        line["qty"]       = orderQty;
        line["unitPrice"] = e["unitPrice"];
        items.append(line);
    }

    if (items.isEmpty()) {
        if (!missingVendor.isEmpty())
            emit errorOccurred("Set a preferred vendor for these low-stock items in Item Master: " +
                               missingVendor.join(", "));
        else
            emit errorOccurred("No low-stock item has a positive shortage to order");
        return false;
    }

    const QString expected = QDateTime::currentDateTime().addDays(7).toString("yyyy-MM-dd");
    const QString poNo = m_po->createWithItems(items, expected, expected, m_session->currentUser());
    if (poNo.isEmpty()) return false;

    if (!missingVendor.isEmpty())
        emit errorOccurred("PO " + poNo + " created. Set a preferred vendor in Item Master for: " +
                           missingVendor.join(", "));

    qDebug() << "Auto-generated" << poNo << "for" << items.size() << "low stock items";
    return true;
}
