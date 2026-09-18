#include "domain/vendorservice.h"

#include "core/dbmanager.h"
#include "core/dbschema.h"

#include <QDebug>

void VendorService::load()
{
    m_vendors.clear();
    if (!m_db) return;
    m_vendors = dbRowsToApp(kVendorFields, m_db->selectAll("vendors", "vendor_name"));
    qDebug() << "Loaded" << m_vendors.size() << "vendors";
}

bool VendorService::save()
{
    if (!m_db) return false;
    if (!m_db->replaceAll("vendors", appRowsToDb(kVendorFields, m_vendors))) {
        emit errorOccurred("Failed to save vendor data");
        return false;
    }
    qDebug() << "Saved" << m_vendors.size() << "vendors";
    return true;
}

bool VendorService::add(const QVariantMap &vendor)
{
    QString vendorName = vendor.value("vendorName").toString();
    qDebug() << "vendor Name:" << vendorName;
    if (vendorName.trimmed().isEmpty()) {
        emit errorOccurred("Vendor name is required");
        return false;
    }

    // Check duplicate
    for (const auto &v : m_vendors) {
        if (v["vendorName"].toString().toLower() == vendorName.trimmed().toLower()) {
            emit errorOccurred("Vendor '" + vendorName + "' already exists");
            return false;
        }
    }

    qDebug()<<"Adding vendor:" << vendor;
    m_vendors.append(vendor);
    qDebug() << "Vendor added to list:" << vendor;
    qDebug() << "Current vendor count:" << m_vendors.size();
    if (!save()) {
        m_vendors.removeLast();
        return false;
    }
    qDebug() << "Vendor saved to file:" << vendor;
    emit listChanged();

    qDebug() << "Added vendor:" << vendorName;
    return true;
}

bool VendorService::update(const QVariantMap &vendor)
{
    QString originalName = vendor.value("originalName").toString().trimmed();
    QString newName = vendor.value("vendorName").toString().trimmed();
    if (newName.isEmpty()) {
        emit errorOccurred("Vendor name is required");
        return false;
    }

    int index = -1;
    auto matchesName = [](const QVariantMap &item, const QString &name) {
        return item.value("vendorName").toString().trimmed().toLower() == name.trimmed().toLower();
    };

    if (!originalName.isEmpty()) {
        for (int i = 0; i < m_vendors.size(); ++i) {
            if (matchesName(m_vendors[i], originalName)) {
                index = i;
                break;
            }
        }
    }

    if (index == -1) {
        for (int i = 0; i < m_vendors.size(); ++i) {
            if (matchesName(m_vendors[i], newName)) {
                index = i;
                break;
            }
        }
    }

    if (index == -1) {
        emit errorOccurred("Vendor not found: " + (originalName.isEmpty() ? newName : originalName));
        return false;
    }

    QString newNameLower = newName.toLower();
    for (int i = 0; i < m_vendors.size(); ++i) {
        if (i == index) continue;
        if (m_vendors[i]["vendorName"].toString().trimmed().toLower() == newNameLower) {
            emit errorOccurred("Vendor '" + newName + "' already exists");
            return false;
        }
    }

    QVariantMap updated = m_vendors[index];
    updated["vendorName"] = newName;
    updated["vendorAddress"] = vendor.value("vendorAddress", updated.value("vendorAddress")).toString();
    updated["bankBranch"] = vendor.value("bankBranch", updated.value("bankBranch")).toString();
    updated["ifsc"] = vendor.value("ifsc", updated.value("ifsc")).toString();
    updated["accountNumber"] = vendor.value("accountNumber", updated.value("accountNumber")).toString();
    updated["cin"] = vendor.value("cin", updated.value("cin")).toString();
    updated["gstin"] = vendor.value("gstin", updated.value("gstin")).toString();
    updated["panNumber"] = vendor.value("panNumber", updated.value("panNumber")).toString();
    updated["panName"] = vendor.value("panName", updated.value("panName")).toString();
    updated["contactPerson"] = vendor.value("contactPerson", updated.value("contactPerson")).toString();
    updated["email"] = vendor.value("email", updated.value("email")).toString();
    updated["phone"] = vendor.value("phone", updated.value("phone")).toString();
    updated["department"] = vendor.value("department", updated.value("department")).toString();
    updated["itemCategory"] = vendor.value("itemCategory", updated.value("itemCategory")).toString();

    QVariantMap previous = m_vendors[index];
    m_vendors[index] = updated;

    if (!save()) {
        m_vendors[index] = previous;
        return false;
    }

    emit listChanged();
    return true;
}

bool VendorService::remove(const QString &name)
{
    for (int i = 0; i < m_vendors.size(); ++i) {
        if (m_vendors[i]["vendorName"].toString().toLower() == name.trimmed().toLower()) {
            QVariantMap removedVendor = m_vendors[i];
            m_vendors.removeAt(i);
            if (!save()) {
                m_vendors.insert(i, removedVendor);
                return false;
            }
            emit listChanged();
            return true;
        }
    }
    emit errorOccurred("Vendor not found: " + name);
    return false;
}

QVariantList VendorService::list()
{
    QVariantList list;
    for (const auto &v : m_vendors) {
        list.append(v);
    }
    return list;
}

QStringList VendorService::names()
{
    QStringList names;
    for (const auto &v : m_vendors) {
        names.append(v["vendorName"].toString());
    }
    return names;
}

QVariantMap VendorService::byName(const QString &name)
{
    for (const auto &v : m_vendors) {
        if (v["vendorName"].toString().toLower() == name.trimmed().toLower()) {
            return v;
        }
    }
    return QVariantMap();
}
