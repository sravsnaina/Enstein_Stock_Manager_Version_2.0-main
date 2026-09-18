#ifndef DOMAIN_PURCHASEREQUESTSERVICE_H
#define DOMAIN_PURCHASEREQUESTSERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class ItemMasterService;

// =============================================================================
//  PurchaseRequestService - what the floor needs bought
// =============================================================================
//
//  A request is what someone on the floor needs bought. Everyone can see the
//  queue; the supply chain team approves or rejects, and turns an approved
//  request into a purchase order. The request keeps the order number it became,
//  so neither side of that hand-off is ever guesswork.
//
//  Anyone can raise a request. The supply chain team reviews it and turns an
//  approved one into a purchase order; linkToPO() records which order it
//  became, so a request and its order can always be traced to each other.
//
//  Only a Pending request may be edited, so nothing shifts under a reviewer
//  after they have acted on it.
//
//  Stored as a header row plus its lines. Lines are rewritten wholesale on
//  every edit rather than diffed.
// =============================================================================
class PurchaseRequestService : public Service
{
    Q_OBJECT

public:
    PurchaseRequestService(const AppContext &ctx, ItemMasterService *itemMaster,
                           QObject *parent = nullptr)
        : Service(ctx, parent), m_itemMaster(itemMaster) {}

    void load();

    // request: department, neededBy, priority, remarks, requestedBy
    // items:   {itemName, partNo, qty, unit, estimatedPrice, vendor}
    QString create(const QVariantMap &request, const QVariantList &items);
    bool update(const QString &prNo, const QVariantMap &request,
                const QVariantList &items);
    bool remove(const QString &prNo);
    // Approve or reject, recording who decided and why.
    bool setStatus(const QString &prNo, const QString &newStatus,
                   const QString &reviewedBy = QString(),
                   const QString &note = QString());
    // Called once the order exists, so a failed order never leaves a request
    // marked as bought.
    bool linkToPO(const QString &prNo, const QString &poNo);

    QVariantList list(const QString &statusFilter = QString());
    // prNo -> one lowercase blob of everything the list can be searched by,
    // including the line items the "+N more" row label hides.
    QVariantMap searchIndex() const;
    QVariantList itemsFor(const QString &prNo);
    QVariantMap byNumber(const QString &prNo);
    QString nextNumber();

    int pendingCount() const;

signals:
    void listChanged();
    void created(const QString &prNo);

private:
    void save();
    void loadItems();
    // Recomputes a header's line count, quantity, estimated value and row
    // label from its lines.
    void recalcHeader(QVariantMap &pr);
    void saveItemsFor(const QString &prNo, const QVariantList &items);

    ItemMasterService *m_itemMaster;

    QVector<QVariantMap> m_requests;
    QVector<QVariantMap> m_prItems;   // line items of all requests
};

#endif // DOMAIN_PURCHASEREQUESTSERVICE_H
