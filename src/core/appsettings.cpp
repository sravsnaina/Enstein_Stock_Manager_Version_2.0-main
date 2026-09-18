#include "core/appsettings.h"

#include "core/dbmanager.h"

namespace {

// The counter keys as they have always been stored in QSettings. Existing
// installations have these exact names, so the mapping is not cosmetic.
const QHash<QString, QString> kSettingsKey = {
    {QStringLiteral("po"),  QStringLiteral("nextPONumber")},
    {QStringLiteral("grn"), QStringLiteral("nextGRNNumber")},
    {QStringLiteral("iss"), QStringLiteral("nextIssueNumber")},
    {QStringLiteral("dc"),  QStringLiteral("nextDCNumber")},
    {QStringLiteral("pr"),  QStringLiteral("nextPRNumber")},
};

} // namespace

QSettings appSettings()
{
    return QSettings(QStringLiteral("EinsteinRobotics"), QStringLiteral("StockManager"));
}

// ---- Counters ---------------------------------------------------------------

int Counters::next(const QString &name)
{
    // The database is the authority whenever there is one: it is what stops two
    // machines on a shared server from minting the same number.
    if (m_db) return m_db->nextCounter(name);

    int &value = m_fallback[name];
    if (value <= 0) value = 1;
    return value++;
}

int Counters::peek(const QString &name)
{
    if (m_db) return m_db->peekCounter(name);

    const int value = m_fallback.value(name, 1);
    return value <= 0 ? 1 : value;
}

void Counters::load()
{
    QSettings settings = appSettings();
    for (auto it = kSettingsKey.constBegin(); it != kSettingsKey.constEnd(); ++it)
        m_fallback[it.key()] = settings.value(it.value(), 1).toInt();
}

void Counters::save() const
{
    QSettings settings = appSettings();
    for (auto it = kSettingsKey.constBegin(); it != kSettingsKey.constEnd(); ++it)
        settings.setValue(it.value(), m_fallback.value(it.key(), 1));
}

// ---- Company profile --------------------------------------------------------

QVariantMap loadCompanyProfile()
{
    QSettings settings = appSettings();
    settings.beginGroup(QStringLiteral("company"));
    QVariantMap p;
    p["name"]         = settings.value("name", "Enstein Robots and Automations Pvt Limited");
    p["addressLine1"] = settings.value("addressLine1", "");
    p["addressLine2"] = settings.value("addressLine2", "");
    p["city"]         = settings.value("city", "");
    p["phone"]        = settings.value("phone", "");
    p["email"]        = settings.value("email", "");
    p["website"]      = settings.value("website", "");
    p["gstin"]        = settings.value("gstin", "");
    settings.endGroup();
    return p;
}

bool saveCompanyProfile(const QVariantMap &profile)
{
    QSettings settings = appSettings();
    settings.beginGroup(QStringLiteral("company"));
    for (const QString &key : {"name", "addressLine1", "addressLine2", "city",
                               "phone", "email", "website", "gstin"}) {
        settings.setValue(key, profile.value(key).toString().trimmed());
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}
