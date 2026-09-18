#ifndef DOMAIN_LOWSTOCKSERVICE_H
#define DOMAIN_LOWSTOCKSERVICE_H

#include "domain/service.h"

#include <QVariantList>

class ItemMasterService;
class PurchaseOrderService;
class StockService;
class VendorService;

// =============================================================================
//  LowStockService - what we are about to run out of
// =============================================================================
//  An item is low when what is on the shelf, plus what is already on order,
//  cannot cover the required quantity set for it in the item master. Counting
//  open orders is what stops the same shortage being ordered twice: an item
//  already on its way is not short.
//
//  This owns no rows of its own. It is a question asked across three services,
//  which is why it holds all three and none of them holds it.
// =============================================================================
class LowStockService : public Service
{
    Q_OBJECT

public:
    LowStockService(const AppContext &ctx, ItemMasterService *itemMaster,
                    StockService *stock, PurchaseOrderService *po,
                    VendorService *vendors,
                    QObject *parent = nullptr)
        : Service(ctx, parent), m_itemMaster(itemMaster), m_stock(stock), m_po(po),
          m_vendors(vendors) {}

    // Each row carries the shortfall as well as the figures behind it, so the
    // alert screen can explain itself.
    QVariantList items();
    int count();

    // Raises one draft order covering every shortage, grouped by preferred
    // vendor. False when there is nothing to order.
    bool autoGeneratePO();

private:
    ItemMasterService *m_itemMaster;
    StockService *m_stock;
    PurchaseOrderService *m_po;
    VendorService *m_vendors;
};

#endif // DOMAIN_LOWSTOCKSERVICE_H
