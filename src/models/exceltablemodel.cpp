#include "models/exceltablemodel.h"

ExcelTableModel::ExcelTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ExcelTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_data.size();
}

int ExcelTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid() || m_data.isEmpty()) return 0;
    return m_data.first().size();
}

QVariant ExcelTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) return QVariant();
    if (index.row() >= m_data.size() || index.column() >= m_data[index.row()].size())
        return QVariant();
    return m_data[index.row()][index.column()];
}

bool ExcelTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;
    if (index.row() >= m_data.size() || index.column() >= m_data[index.row()].size())
        return false;
    m_data[index.row()][index.column()] = value;
    emit dataChanged(index, index, {role});
    return true;
}

Qt::ItemFlags ExcelTableModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QHash<int, QByteArray> ExcelTableModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    return roles;
}

void ExcelTableModel::setExcelData(const QVector<QVector<QVariant>> &data)
{
    beginResetModel();
    m_data = data;
    endResetModel();
}

QVector<QVector<QVariant>> ExcelTableModel::getExcelData() const
{
    return m_data;
}

QVariant ExcelTableModel::getData(int row, int column) const
{
    if (row < 0 || row >= m_data.size()) return QVariant();
    if (column < 0 || column >= m_data[row].size()) return QVariant();
    return m_data[row][column];
}

bool ExcelTableModel::setDataAt(int row, int column, const QVariant &value)
{
    if (row < 0 || row >= m_data.size()) return false;
    if (column < 0 || column >= m_data[row].size()) return false;
    m_data[row][column] = value;
    QModelIndex idx = index(row, column);
    emit dataChanged(idx, idx, {Qt::DisplayRole});
    return true;
}

void ExcelTableModel::addRow()
{
    int cols = m_data.isEmpty() ? 9 : m_data.first().size();
    beginInsertRows(QModelIndex(), m_data.size(), m_data.size());
    QVector<QVariant> newRow(cols);
    m_data.append(newRow);
    endInsertRows();
}

bool ExcelTableModel::removeRowAt(int row)
{
    if (row <= 0 || row >= m_data.size()) return false;
    beginRemoveRows(QModelIndex(), row, row);
    m_data.removeAt(row);
    endRemoveRows();
    return true;
}

void ExcelTableModel::addColumn()
{
    if (m_data.isEmpty()) return;
    int cols = m_data.first().size();
    beginInsertColumns(QModelIndex(), cols, cols);
    for (auto &row : m_data) {
        row.append(QVariant());
    }
    endInsertColumns();
}

void ExcelTableModel::clear()
{
    beginResetModel();
    m_data.clear();
    endResetModel();
}
