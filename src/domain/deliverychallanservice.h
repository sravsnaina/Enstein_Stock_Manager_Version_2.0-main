#ifndef DOMAIN_DELIVERYCHALLANSERVICE_H
#define DOMAIN_DELIVERYCHALLANSERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class ItemMasterService;

// =============================================================================
//  DeliveryChallanService - goods leaving the premises
// =============================================================================
//
//  A challan is the note that travels with goods leaving the premises. It
//  records what was handed over and to whom, and prints onto the company's
//  paper template; it deliberately does not move stock, so it can never
//  double-count against Issue Stock or a GRN.
//
//  A challan is stored as a header row plus its lines, the same shape as a
//  purchase order. Lines are rewritten wholesale on every edit rather than
//  diffed, which is why saveItemsFor() replaces rather than merges.
//
//  Reads the item master to fill in a line's part number, HSN/SAC code and
//  unit when the user leaves them blank.
// =============================================================================
class DeliveryChallanService : public Service
{
    Q_OBJECT

public:
    DeliveryChallanService(const AppContext &ctx, ItemMasterService *itemMaster,
                           QObject *parent = nullptr)
        : Service(ctx, parent), m_itemMaster(itemMaster) {}

    void load();

    // challan: date, deliveryTime, party* and ship* blocks, terms, preparedBy...
    // items:   {itemName, partNo, hsnCode, qty, unit}
    // Returns the new challan number, or "" if it could not be created.
    QString create(const QVariantMap &challan, const QVariantList &items);
    bool update(const QString &dcNo, const QVariantMap &challan,
                const QVariantList &items);
    bool remove(const QString &dcNo);
    bool setStatus(const QString &dcNo, const QString &newStatus);

    QVariantList list(const QString &statusFilter = QString());
    // dcNo -> one lowercase blob of everything the list can be searched by,
    // including the line items the "+N more" row label hides.
    QVariantMap searchIndex() const;
    QVariantList itemsFor(const QString &dcNo);
    QVariantMap byNumber(const QString &dcNo);
    // The number the next challan would get, without consuming it.
    QString nextNumber();

    // Parties challans have been raised for, for the "deliver to" picker.
    QVariantList partyList() const;

    // Renders to a PDF in a temp folder and rasterises its pages for the
    // preview dialog: { dcNo, pdfPath, pages: [image urls] }.
    QVariantMap generatePreview(const QString &dcNo);
    // Writes the PDF to its permanent home; empty destPath uses
    // defaultPdfPath(). Returns the saved path, or "" on failure.
    QString savePdf(const QString &dcNo, const QString &destPath = QString());
    QString defaultPdfPath(const QString &dcNo) const;

signals:
    void listChanged();
    void created(const QString &dcNo);

private:
    void save();
    void loadItems();
    // Recomputes a header's line count, total quantity and row label from its
    // lines, so the list never disagrees with what is inside a challan.
    void recalcHeader(QVariantMap &dc);
    // Writes one challan's lines, replacing whatever it had before.
    void saveItemsFor(const QString &dcNo, const QVariantList &items);
    QString buildHtml(const QString &dcNo) const;

    ItemMasterService *m_itemMaster;

    QVector<QVariantMap> m_challans;
    QVector<QVariantMap> m_dcItems;   // line items of all challans
};

#endif // DOMAIN_DELIVERYCHALLANSERVICE_H
