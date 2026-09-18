#ifndef DOMAIN_PURCHASEORDERSERVICE_H
#define DOMAIN_PURCHASEORDERSERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class ItemMasterService;
class StockMovementService;
class StockService;
class VendorService;

// =============================================================================
//  PurchaseOrderService - what we have ordered and from whom
// =============================================================================
//  An order is a header row plus its line items, each line carrying its own
//  part, quantity, price and vendor. The header's totals are never stored
//  independently: recalcHeader() derives them from the lines every time they
//  change, so the list can never disagree with what is inside an order.
//
//  Orders move Draft -> Sent -> Partially Received -> Received. Receiving is
//  not done here; GoodsReceiptService owns that and writes the received
//  quantities back through this service.
//
//  Reads the item master to fill in a line the user left blank, and the vendor
//  master for the address block on the printed order. A PO raised against one
//  vendor gets that vendor's full details; a mixed-vendor order can only
//  sensibly list the names.
// =============================================================================
class PurchaseOrderService : public Service
{
    Q_OBJECT

public:
    PurchaseOrderService(const AppContext &ctx, ItemMasterService *itemMaster,
                         VendorService *vendors, StockMovementService *movements,
                         QObject *parent = nullptr)
        : Service(ctx, parent), m_itemMaster(itemMaster), m_vendorService(vendors),
          m_movements(movements) {}

    void load();

    // One order holding several lines; each line carries its own vendor.
    // items: {partName, partNo, vendor, department, qty, unitPrice}
    QString createWithItems(const QVariantList &items,
                            const QString &expectedDate,
                            const QString &expectedEndDate,
                            const QString &preparedBy = QString());
    // The older single-line entry point, kept because QML still offers it;
    // it builds a one-line list and calls createWithItems().
    QString create(const QString &vendor, const QString &partName,
                   const QString &partNo, int qty, double unitPrice,
                   const QString &expectedDate,
                   const QString &department = QString(),
                   const QString &preparedBy = QString(),
                   const QString &expectedEndDate = QString());

    bool update(const QString &poNo, const QVariantMap &poDetails);
    bool setStatus(const QString &poNo, const QString &newStatus);
    bool sendForApproval(const QString &poNo, const QString &approvedBy);

    QVariantList list(const QString &statusFilter = QString());
    // poNo -> one lowercase blob of everything the list can be searched by.
    QVariantMap searchIndex() const;
    QVariantList itemsFor(const QString &poNo);
    QVariantMap byNumber(const QString &poNo);
    QString nextNumber();

    // Draft, Sent or Partially Received - the orders still owed to us.
    int pendingCount() const;

    QVariantMap generatePreview(const QString &poNo, const QString &comments);
    QString savePdf(const QString &poNo, const QString &comments,
                    const QString &destPath = QString());
    QString defaultPdfPath(const QString &poNo) const;

    // For GoodsReceiptService, which writes received quantities back onto the
    // lines and then asks this service to recompute the header.
    QVector<QVariantMap> &orders() { return m_orders; }
    QVector<QVariantMap> &lineItems() { return m_poItems; }
    void recalcHeader(QVariantMap &po);
    void save();

signals:
    void listChanged();
    void created(const QString &poNo);

private:
    void loadItems();
    // Fills a line's blanks from the item master; false means the line failed
    // validation and an error has already been emitted.
    bool resolveLine(QVariantMap &line);
    QString buildHtml(const QString &poNo, const QString &comments) const;

    ItemMasterService *m_itemMaster;
    VendorService *m_vendorService;
    StockMovementService *m_movements;

    QVector<QVariantMap> m_orders;
    QVector<QVariantMap> m_poItems;   // line items of all orders
};

#endif // DOMAIN_PURCHASEORDERSERVICE_H
