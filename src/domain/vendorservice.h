#ifndef DOMAIN_VENDORSERVICE_H
#define DOMAIN_VENDORSERVICE_H

#include "domain/service.h"

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

// =============================================================================
//  VendorService - who we buy from
// =============================================================================
//  The vendor master: names, addresses, bank and tax details. A vendor is
//  identified by name throughout the application, which is why update() takes
//  the original name alongside the new one and why renaming to a name already
//  in use is refused.
//
//  Purchase orders and delivery challans read vendors from here; nothing is
//  read back the other way, so this service depends on nothing but the
//  database.
// =============================================================================
class VendorService : public Service
{
    Q_OBJECT

public:
    explicit VendorService(const AppContext &ctx, QObject *parent = nullptr)
        : Service(ctx, parent) {}

    // Fills the in-memory list from the database. Called once at startup and
    // again whenever another machine's changes are pulled in.
    void load();

    bool add(const QVariantMap &vendor);
    // vendor["originalName"] names the row to change; everything else replaces
    // what is there.
    bool update(const QVariantMap &vendor);
    bool remove(const QString &name);

    QVariantList list();
    QStringList names();
    // Case-insensitive; an empty map means no such vendor.
    QVariantMap byName(const QString &name);

    int count() const { return m_vendors.size(); }

signals:
    // A vendor was added, changed or removed: anything showing the list or a
    // vendor dropdown needs to refresh.
    void listChanged();

private:
    bool save();

    QVector<QVariantMap> m_vendors;
};

#endif // DOMAIN_VENDORSERVICE_H
