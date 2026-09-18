#ifndef DOMAIN_ITEMMASTERSERVICE_H
#define DOMAIN_ITEMMASTERSERVICE_H

#include "domain/service.h"

#include <QVariantList>
#include <QVariantMap>
#include <QVector>

// =============================================================================
//  ItemMasterService - the catalogue of everything we buy and stock
// =============================================================================
//  One row per part: what it is called, its part number, department, preferred
//  vendor, unit price and required quantity, and how it is numbered for tax.
//
//  An item is either a tangible good, numbered with an HSN code, or an
//  intangible service, numbered with a SAC code. Both codes are stored, and
//  neither is cleared when the classification changes, so reclassifying an item
//  and changing your mind does not discard the number already typed.
//
//  Purchase orders, delivery challans, purchase requests and the low-stock
//  check all read from here to fill in blanks the user would otherwise retype.
//  Nothing is read back the other way.
// =============================================================================
class ItemMasterService : public Service
{
    Q_OBJECT

public:
    explicit ItemMasterService(const AppContext &ctx, QObject *parent = nullptr)
        : Service(ctx, parent) {}

    void load();

    bool add(QVariantMap itemDetails);
    // itemDetails["originalPartNo"] names the row to change. Part numbers are
    // unique, so renaming onto one already in use is refused.
    bool update(QVariantMap itemDetails);
    bool removeByPartName(const QString &partName);

    // For QML. Each row carries an extra "taxCode" key resolved from the
    // classification, so a screen never has to decide HSN-or-SAC itself.
    QVariantList list();

    // For other services, which fill blanks on their own lines from these
    // rows. Read-only by contract: this service owns them.
    const QVector<QVariantMap> &rows() const { return m_items; }

    int count() const { return m_items.size(); }

signals:
    // An item was added, changed or removed. The low-stock count is derived
    // from these rows, so it is stale after this too - the bridge fans this
    // one signal out to both of the names QML binds to.
    void listChanged();

private:
    void save();

    QVector<QVariantMap> m_items;
};

// The single number a printed document shows for an item master row: the HSN
// code for a good, the SAC code for a service, falling back to whichever was
// actually filled in. Free rather than a member because the delivery challan
// resolver works on plain rows.
QString itemTaxCode(const QVariantMap &item);

#endif // DOMAIN_ITEMMASTERSERVICE_H
