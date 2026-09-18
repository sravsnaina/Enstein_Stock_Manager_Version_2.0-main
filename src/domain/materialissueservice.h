#ifndef DOMAIN_MATERIALISSUESERVICE_H
#define DOMAIN_MATERIALISSUESERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

class StockMovementService;
class StockService;

// =============================================================================
//  MaterialIssueService - stock going out to a department
// =============================================================================
//  An issue note takes quantity off the shelf and records who it went to. One
//  note can cover several parts, which is why issueMany() is the real entry
//  point and issueOne() is a convenience over it.
//
//  Issuing is refused outright if any line would take a part below zero. That
//  check happens across the whole note before anything is written, so a
//  multi-part issue either happens completely or not at all - a half-applied
//  issue would leave the shelf and the screen disagreeing with no record why.
// =============================================================================
class MaterialIssueService : public Service
{
    Q_OBJECT

public:
    MaterialIssueService(const AppContext &ctx, StockService *stock,
                         StockMovementService *movements, QObject *parent = nullptr)
        : Service(ctx, parent), m_stock(stock), m_movements(movements) {}

    void load();

    // items: {partName, qty}. Returns the new issue number, or "" if the note
    // was refused - in which case nothing was changed.
    QString issueMany(const QVariantList &items, const QString &department,
                      const QString &issuedBy);
    QString issueOne(const QString &partName, int qty,
                     const QString &department, const QString &issuedBy);

    // Newest first.
    QVariantList list();

    // For the report export. Read-only by contract.
    const QVector<QVariantMap> &rows() const { return m_notes; }

signals:
    void issued(const QString &issueNo, const QString &partName, int qty);

private:
    void save();

    StockService *m_stock;
    StockMovementService *m_movements;

    QVector<QVariantMap> m_notes;
};

#endif // DOMAIN_MATERIALISSUESERVICE_H
