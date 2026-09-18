#ifndef DOMAIN_STOCKMOVEMENTSERVICE_H
#define DOMAIN_STOCKMOVEMENTSERVICE_H

#include "domain/service.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

// =============================================================================
//  StockMovementService - the audit trail
// =============================================================================
//  Every quantity change writes a line here: what moved, how much, which way,
//  and what caused it. Receiving against a GRN, issuing to a department and
//  correcting the grid by hand all pass through log().
//
//  This is append-only by design. It is the record you reach for when the
//  number on the shelf and the number on the screen disagree, so nothing edits
//  or deletes what is already written.
//
//  Deliberately depends on nothing: it is told what happened rather than
//  working it out, which keeps it usable from any service.
// =============================================================================
class StockMovementService : public Service
{
    Q_OBJECT

public:
    explicit StockMovementService(const AppContext &ctx, QObject *parent = nullptr)
        : Service(ctx, parent) {}

    void load();

    // movementType is "IN", "OUT" or "ADJUST"; reference names the document
    // that caused it (a GRN number, an issue number) so a line can be traced.
    bool log(const QString &partName, const QString &partNo,
             const QString &movementType, int qty,
             const QString &reference, const QString &doneBy);

    // Empty filter returns everything, newest first.
    QVariantList forPart(const QString &partNameFilter = QString());
    QVariantList all();

    // For the report export, which writes the raw log to a spreadsheet.
    // Read-only by contract: this service owns them.
    const QVector<QVariantMap> &rows() const { return m_movements; }

    int count() const { return m_movements.size(); }

signals:
    void logged(const QString &partName, const QString &type, int qty);

private:
    void save();

    QVector<QVariantMap> m_movements;
};

#endif // DOMAIN_STOCKMOVEMENTSERVICE_H
