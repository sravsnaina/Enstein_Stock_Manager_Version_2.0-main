#ifndef MODELS_EXCELTABLEMODEL_H
#define MODELS_EXCELTABLEMODEL_H

#include <QAbstractTableModel>
#include <QVariant>
#include <QVector>

// =============================================================================
//  ExcelTableModel - the stock grid
// =============================================================================
//  A plain rows-and-columns table backing the main stock view. Row 0 holds the
//  column headings, exactly as a spreadsheet does, which is why the QML grid
//  starts its data at row 1 and why dataRows is rowCount() - 1.
//
//  This is a dumb container on purpose: it stores cells and notifies views, and
//  knows nothing about parts, stock levels or the database. Loading it and
//  writing it back is the bridge's job.
// =============================================================================

class ExcelTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit ExcelTableModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setExcelData(const QVector<QVector<QVariant>> &data);
    QVector<QVector<QVariant>> getExcelData() const;

    Q_INVOKABLE QVariant getData(int row, int column) const;
    Q_INVOKABLE bool setDataAt(int row, int column, const QVariant &value);
    Q_INVOKABLE void addRow();
    Q_INVOKABLE bool removeRowAt(int row);
    Q_INVOKABLE void addColumn();
    Q_INVOKABLE void clear();

private:
    QVector<QVector<QVariant>> m_data;
};

#endif // MODELS_EXCELTABLEMODEL_H
