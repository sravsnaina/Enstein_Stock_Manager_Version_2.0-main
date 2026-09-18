#include "domain/materialissueservice.h"

#include "core/appsettings.h"
#include "core/dbmanager.h"
#include "core/dbschema.h"
#include "domain/stockmovementservice.h"
#include "domain/stockservice.h"
#include "models/exceltablemodel.h"

#include <QDateTime>
#include <QDebug>

// Stock cells can hold a quantity typed as text, so every read goes here.
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

void MaterialIssueService::load()
{
    m_notes.clear();
    if (!m_db) return;
    m_notes = dbRowsToApp(kIssueFields, m_db->selectAll("issue_notes", "issue_no"));
    qDebug() << "Loaded" << m_notes.size() << "issue notes";
}

void MaterialIssueService::save()
{
    if (!m_db) return;
    m_db->replaceAll("issue_notes", appRowsToDb(kIssueFields, m_notes));
}

QString MaterialIssueService::issueMany(const QVariantList &items,
                                         const QString &department,
                                         const QString &issuedBy)
{
    if (items.isEmpty()) {
        emit errorOccurred("Add at least one part to issue");
        return "";
    }
    if (department.trimmed().isEmpty()) {
        emit errorOccurred("Department is required");
        return "";
    }

    // Validate ALL lines before touching any stock (all-or-nothing).
    struct IssueLine { int row; QString partName; int qty; };
    QVector<IssueLine> lines;
    for (const QVariant &v : items) {
        const QVariantMap item = v.toMap();
        const QString partName = item["partName"].toString().trimmed();
        const int qty = item["qty"].toInt();

        if (partName.isEmpty()) {
            emit errorOccurred("Part name is required for every issue line");
            return "";
        }
        if (qty <= 0) {
            emit errorOccurred("Quantity must be greater than 0 for: " + partName);
            return "";
        }
        int partRow = m_stock->findRowByName(partName);
        if (partRow == -1) {
            emit errorOccurred("Part not found in stock: " + partName);
            return "";
        }
        int currentStock = toStockInt(m_model->getData(partRow, 2));
        // Account for earlier lines of this same request drawing on one part.
        for (const IssueLine &prev : lines)
            if (prev.row == partRow) currentStock -= prev.qty;
        if (currentStock < qty) {
            emit errorOccurred("Insufficient stock for " + partName +
                               "! Available: " + QString::number(currentStock) +
                               ", Requested: " + QString::number(qty));
            return "";
        }
        lines.append({partRow, partName, qty});
    }

    // Generate one issue number (atomic, shared) for the whole note.
    int issSeq = m_counters->next("iss");
    QString issueNo = "ISS-" + QString::number(issSeq).rightJustified(4, '0');
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");

    for (const IssueLine &line : lines) {
        // Deduct stock
        int currentStock = toStockInt(m_model->getData(line.row, 2));
        m_model->setDataAt(line.row, 2, currentStock - line.qty);

        // Record the issue line (append-only insert; one issue number can
        // hold several lines in the v2 schema).
        QVariantMap note;
        note["issueNo"]    = issueNo;
        note["date"]       = stamp;
        note["partName"]   = line.partName;
        note["qty"]        = line.qty;
        note["department"] = department;
        note["issuedBy"]   = issuedBy;
        m_notes.append(note);
        if (m_db) m_db->insert("issue_notes", appRowToDb(kIssueFields, note));

        // Log movement
        QString partNo = m_model->getData(line.row, 1).toString();
        m_movements->log(line.partName, partNo, "OUT", line.qty,
                         issueNo + " -> " + department, issuedBy);

        emit issued(issueNo, line.partName, line.qty);
        qDebug() << "Issued:" << line.partName << "x" << line.qty
                 << "to" << department << "(" << issueNo << ")";
    }

    // Persist the updated stock (database is the permanent store).
    m_stock->persist();

    return issueNo;
}

QString MaterialIssueService::issueOne(const QString &partName, int qty,
                                 const QString &department, const QString &issuedBy)
{
    QVariantMap item;
    item["partName"] = partName;
    item["qty"] = qty;
    return issueMany({item}, department, issuedBy);
}

QVariantList MaterialIssueService::list()
{
    QVariantList list;
    for (int i = m_notes.size() - 1; i >= 0; --i) {
        list.append(m_notes[i]);
    }
    return list;
}
