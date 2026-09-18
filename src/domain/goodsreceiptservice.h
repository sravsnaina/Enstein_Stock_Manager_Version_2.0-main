#ifndef DOMAIN_GOODSRECEIPTSERVICE_H
#define DOMAIN_GOODSRECEIPTSERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class PurchaseOrderService;
class StockMovementService;
class StockService;

// =============================================================================
//  GoodsReceiptService - goods arriving against an order
// =============================================================================
//  A GRN records what actually turned up: how much was received, how much was
//  accepted, and how much was rejected. Only the accepted quantity reaches the
//  shelf; the rejected quantity is logged as a movement so there is a record of
//  it, but it never becomes stock.
//
//  Receiving is the one operation that touches three domains at once, which is
//  why this service sits downstream of all of them: it writes the received
//  quantity back onto the purchase order line, adds accepted stock to the grid,
//  and logs a movement for each. An order becomes Received only when every one
//  of its lines is fully received; until then it is Partially Received.
// =============================================================================
class GoodsReceiptService : public Service
{
    Q_OBJECT

public:
    GoodsReceiptService(const AppContext &ctx, PurchaseOrderService *po,
                        StockService *stock, StockMovementService *movements,
                        QObject *parent = nullptr)
        : Service(ctx, parent), m_po(po), m_stock(stock), m_movements(movements) {}

    void load();

    // Receive against one line of a multi-line order. Returns the new GRN
    // number, or "" if nothing was received.
    QString receiveForItem(int itemId, int receivedQty, int acceptedQty,
                           int rejectedQty, const QString &remarks,
                           const QString &receivedBy = QString());
    // Older whole-order entry point: receives against the first line still
    // open. Kept because QML still offers it.
    QString receiveForOrder(const QString &poNo, int receivedQty, int acceptedQty,
                            int rejectedQty, const QString &remarks,
                            const QString &receivedBy = QString());

    QVariantList list();

    // For the report export. Read-only by contract.
    const QVector<QVariantMap> &rows() const { return m_records; }

signals:
    // Goods reached the shelf. The order's pending count and the low-stock
    // figure both move as a result, which the bridge fans out.
    void received(const QString &grnNo, const QString &poNo);

private:
    void save();

    PurchaseOrderService *m_po;
    StockService *m_stock;
    StockMovementService *m_movements;

    QVector<QVariantMap> m_records;
};

#endif // DOMAIN_GOODSRECEIPTSERVICE_H
