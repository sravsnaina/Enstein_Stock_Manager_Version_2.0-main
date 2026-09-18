#include "domain/stockservice.h"

#include "core/dbmanager.h"
#include "models/exceltablemodel.h"

#include <QDebug>
#include <QVariant>

// The dashboard grid columns, in model order 0..8.
static const char *kStockDbColumns[] = {
    "part_name", "part_no", "stock", "department", "prepared",
    "approved", "vendor", "row_date", "unit_price"
};

// Stock quantities are written by hand as well as by the app, so a cell can
// hold "12" as text rather than the number 12. Everything that reads a
// quantity goes through here.
static int toStockInt(const QVariant &value)
{
    bool ok = false;
    double number = value.toDouble(&ok);
    if (!ok) {
        number = value.toString().trimmed().toDouble(&ok);
        if (!ok) return 0;
    }
    return qRound(number);
}

bool StockService::loadFromDb()
{
    if (!m_db) return false;
    const QVector<QVariantMap> rows = m_db->selectAll("stock_rows", "id");
    if (rows.isEmpty()) return false;

    QVector<QVector<QVariant>> data;
    data.reserve(rows.size() + 1);
    data.append({"Part Name", "Part No", "Stock",
                 "Department", "Prepared", "Approved", "Vendor Name",
                 "Date", "Unit Price"});

    for (const QVariantMap &r : rows) {
        QVector<QVariant> row(9);
        for (int c = 0; c < 9; ++c) {
            QVariant v = r.value(QLatin1String(kStockDbColumns[c]));
            // Keep empty cells empty instead of showing 0s.
            if (!v.isNull() && v.toString() != "")
                row[c] = v;
        }
        data.append(row);
    }

    m_model->setExcelData(data);
    emit loaded();
    qDebug() << "Loaded" << rows.size() << "stock rows from database";
    return true;
}

void StockService::saveToDb()
{
    if (!m_db || !m_db->isConnected()) return;

    // The stock grid is stored as a whole table, so writing it out replaces
    // every row. If another machine has written since our last reload, our copy
    // is stale and a blind write would wipe their rows. Refuse, reload, and say
    // so - losing one cell entry with a message beats silently discarding
    // someone else's work.
    if (m_db->isServerBackend() && m_db->hasRemoteChanges()) {
        emit errorOccurred("Someone else changed the stock while you were editing. "
                           "Your last stock edit was not saved - the grid has been "
                           "refreshed with their version, please re-enter it.");
        emit remoteChangesDetected();
        return;
    }

    QVector<QVariantMap> rows;
    const int rowCount = m_model->rowCount();
    for (int r = 1; r < rowCount; ++r) {           // skip header row
        QVariantMap row;
        bool empty = true;
        for (int c = 0; c < 9; ++c) {
            QVariant v = m_model->getData(r, c);
            if (!v.toString().trimmed().isEmpty()) empty = false;
            row[QLatin1String(kStockDbColumns[c])] = v.toString().isEmpty() ? QVariant(QString()) : v;
        }
        if (!empty) rows.append(row);
    }

    m_db->replaceAll("stock_rows", rows);
}

int StockService::findRowByName(const QString &partName)
{
    int rows = m_model->rowCount();
    QString search = partName.trimmed().toLower();

    for (int row = 1; row < rows; ++row) {
        QString existing = m_model->getData(row, 0).toString().trimmed().toLower();
        if (existing == search) {
            return row;
        }
    }
    return -1;
}

void StockService::persist()
{
    saveToDb();
    emit changed();
}
