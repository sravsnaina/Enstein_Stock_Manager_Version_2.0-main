#include "domain/stockmovementservice.h"

#include "core/dbmanager.h"
#include "core/dbschema.h"

#include <QDateTime>
#include <QDebug>

void StockMovementService::load()
{
    m_movements.clear();
    if (!m_db) return;
    m_movements = dbRowsToApp(kMovementFields, m_db->selectAll("stock_movements", "id"));
    qDebug() << "Loaded" << m_movements.size() << "stock movements";
}

void StockMovementService::save()
{
    // The movement log is append-only; individual entries are inserted by
    // log(). A full rewrite is only used as a fallback.
    if (!m_db) return;
    m_db->replaceAll("stock_movements", appRowsToDb(kMovementFields, m_movements));
}

bool StockMovementService::log(const QString &partName, const QString &partNo,
                                    const QString &movementType, int qty,
                                    const QString &reference, const QString &doneBy)
{
    QVariantMap mov;
    mov["date"]      = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    mov["partName"]  = partName;
    mov["partNo"]    = partNo;
    mov["type"]      = movementType;
    mov["qty"]       = qty;
    mov["reference"] = reference;
    mov["doneBy"]    = doneBy;

    m_movements.append(mov);
    if (m_db) m_db->insert("stock_movements", appRowToDb(kMovementFields, mov));
    emit logged(partName, movementType, qty);

    return true;
}

QVariantList StockMovementService::forPart(const QString &partNameFilter)
{
    QVariantList list;
    for (const auto &mov : m_movements) {
        if (partNameFilter.isEmpty() ||
            mov["partName"].toString().toLower().contains(partNameFilter.toLower())) {
            list.append(mov);
        }
    }
    return list;
}

QVariantList StockMovementService::all()
{
    QVariantList list;
    // Return in reverse order (newest first)
    for (int i = m_movements.size() - 1; i >= 0; --i) {
        list.append(m_movements[i]);
    }
    return list;
}
