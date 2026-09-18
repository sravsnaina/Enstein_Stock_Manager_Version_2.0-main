#include "domain/itemmasterservice.h"

#include "core/dbmanager.h"
#include "core/dbschema.h"

#include <QDebug>

// An item is either a tangible good, numbered with an HSN code, or an
// intangible service, numbered with a SAC code.
static const QLatin1String kTangible("Tangible");
static const QLatin1String kIntangible("Intangible");

static bool isIntangibleItem(const QVariantMap &item)
{
    return item.value("itemType").toString().trimmed()
               .compare(kIntangible, Qt::CaseInsensitive) == 0;
}

// Forces a row into one of the two classifications. Rows written before the
// split have no type at all; every item back then was a good, so they come
// back as tangible rather than as some third empty state the UI can't show.
static void normalizeItemClassification(QVariantMap &item)
{
    item["itemType"] = isIntangibleItem(item) ? QString(kIntangible) : QString(kTangible);
    // The code that does not apply is kept, not cleared, so reclassifying an
    // item and changing your mind does not cost you the number you typed.
    if (!item.contains("hsnCode")) item["hsnCode"] = QString();
    if (!item.contains("sacCode")) item["sacCode"] = QString();
}

// The single number a printed document shows for this item. Challans and
// exports have one "HSN/SAC" column, so they ask here instead of each
// re-deciding the goods/services rule.
QString itemTaxCode(const QVariantMap &item)
{
    const bool intangible = isIntangibleItem(item);
    const QString hsn = item.value("hsnCode").toString().trimmed();
    const QString sac = item.value("sacCode").toString().trimmed();
    const QString primary = intangible ? sac : hsn;
    if (!primary.isEmpty()) return primary;
    // An item classified one way but numbered the other still prints its
    // number rather than a blank cell.
    return intangible ? hsn : sac;
}

void ItemMasterService::load()
{
    m_items.clear();
    if (!m_db) return;
    m_items = dbRowsToApp(kItemFields, m_db->selectAll("item_master", "part_no"));
    for (QVariantMap &item : m_items)
        normalizeItemClassification(item);
    qDebug() << "Loaded" << m_items.size() << "items in Item Master";
}

void ItemMasterService::save()
{
    if (!m_db) return;
    m_db->replaceAll("item_master", appRowsToDb(kItemFields, m_items));
}

bool ItemMasterService::add(QVariantMap itemDetails) 
{
    QString partNo = itemDetails["partNo"].toString();
    if (partNo.trimmed().isEmpty()) {
        emit errorOccurred("Part number is required");
        return false;
    }

    // Check duplicate
    for (const auto &item : m_items) {
        if (item["partNo"].toString().toLower() == partNo.trimmed().toLower()) {
            //emit errorOccurred("Part '" + partNo + "' already exists in Item Master");
        }
    }

    normalizeItemClassification(itemDetails);
    m_items.append(itemDetails);
    save();
    emit listChanged();
    return true;
}

bool ItemMasterService::update(QVariantMap itemDetails)
{
    QString originalPartNo = itemDetails.value("originalPartNo").toString().trimmed();
    QString newPartNo = itemDetails.value("partNo").toString().trimmed();
    if (newPartNo.isEmpty()) {
        emit errorOccurred("Part number is required");           
        return false;
    }

    int index = -1;
    auto matchesPartNo = [](const QVariantMap &item, const QString &partNo) {
        return item.value("partNo").toString().trimmed().toLower() == partNo.trimmed().toLower();
    };

    if (!originalPartNo.isEmpty()) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (matchesPartNo(m_items[i], originalPartNo)) {
                index = i;
                break;
            }
        }
    }

    if (index == -1) {
        for (int i = 0; i < m_items.size(); ++i) {
            if (matchesPartNo(m_items[i], newPartNo)) {
                index = i;
                break;
            }
        }
    }

    if (index == -1) {
        emit errorOccurred("Item not found: " + (originalPartNo.isEmpty() ? newPartNo : originalPartNo));
        return false;
    }

    QString newPartNoLower = newPartNo.toLower();
    for (int i = 0; i < m_items.size(); ++i) {
        if (i == index) continue;
        if (m_items[i]["partNo"].toString().trimmed().toLower() == newPartNoLower) {
            emit errorOccurred("Part No '" + newPartNo + "' already exists");
            return false;
        }
    }

    QVariantMap updated = m_items[index];
    QString partName = itemDetails.value("partName", updated.value("partName")).toString();
    QString department = itemDetails.value("department", updated.value("department")).toString();
    int requiredQty = itemDetails.value("requiredQty", updated.value("requiredQty")).toInt();
    double unitPrice = itemDetails.value("unitPrice", updated.value("unitPrice")).toDouble();
    QString vendor = itemDetails.value("vendor", updated.value("vendor")).toString();
    QString hsnCode = itemDetails.value("hsnCode", updated.value("hsnCode")).toString();
    QString unit = itemDetails.value("unit", updated.value("unit")).toString();

    updated["hsnCode"] = hsnCode;
    updated["sacCode"] = itemDetails.value("sacCode", updated.value("sacCode")).toString();
    updated["itemType"] = itemDetails.value("itemType", updated.value("itemType")).toString();
    updated["unit"] = unit;
    updated["partNo"] = newPartNo;
    updated["partName"] = partName;
    updated["department"] = department;
    updated["requiredQty"] = requiredQty;
    updated["unitPrice"] = unitPrice;
    updated["vendor"] = vendor;

    // Keep legacy columns in sync for persistence.
    updated["category"] = itemDetails.value("category", department.isEmpty() ? updated.value("category") : department);
    updated["stockQty"] = itemDetails.contains("stockQty") ? itemDetails.value("stockQty").toInt() : requiredQty;

    normalizeItemClassification(updated);

    QVariantMap previous = m_items[index];
    m_items[index] = updated;

    save();
    emit listChanged();
    return true;
}

QVariantList ItemMasterService::list()
{
    QVariantList list;
    for (const auto &item : m_items) {
        QVariantMap row = item;
        // Resolved here so every screen shows the same number without each
        // one repeating the goods/services rule.
        row["taxCode"] = itemTaxCode(item);
        list.append(row);
    }
    return list;
}

bool ItemMasterService::removeByPartName(const QString &partName)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i]["partName"].toString().toLower() == partName.trimmed().toLower()) {
            m_items.removeAt(i);
            save();
            emit listChanged();
            return true;
        }
    }
    emit errorOccurred("Item not found: " + partName);
    return false;
}
