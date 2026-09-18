#ifndef CORE_APPSETTINGS_H
#define CORE_APPSETTINGS_H

#include <QHash>
#include <QSettings>
#include <QString>
#include <QVariantMap>

class DatabaseManager;

// =============================================================================
//  Settings and shared state that no single domain owns
// =============================================================================
//  Three small things every part of the application reaches for: the document
//  numbering, who is logged in, and the company's own details for letterheads.
//  They live here rather than on one service because none of them belongs to
//  vendors or to orders in particular.
//
//  All three persist under the same QSettings organisation, "EinsteinRobotics"
//  / "StockManager". Changing those strings orphans every existing
//  installation's settings, so don't.
// =============================================================================

// Returns the application's QSettings, opened on the one organisation and
// application name the whole app agrees on.
QSettings appSettings();

// -----------------------------------------------------------------------------
//  Counters - document numbering (PO-0001, DC-0007, ...)
// -----------------------------------------------------------------------------
//  Numbers come from the database so that every machine sharing a server draws
//  from one sequence and two people cannot raise the same PO number. The
//  in-memory fallback only applies when there is no database at all.
//
//  Names are the short keys the database counter table uses: "po", "grn",
//  "iss", "dc", "pr".
class Counters
{
public:
    explicit Counters(DatabaseManager *db) : m_db(db) {}

    // Takes the next number and advances the sequence. Call once, when the
    // document is actually being created - peeking is what a preview wants.
    int next(const QString &name);

    // The number the next document would get, without consuming it. Used to
    // show "PO-0009" on a form before the user commits to it.
    int peek(const QString &name);

    void load();
    void save() const;

private:
    DatabaseManager *m_db;
    QHash<QString, int> m_fallback;
};

// -----------------------------------------------------------------------------
//  Session - who is using the application
// -----------------------------------------------------------------------------
//  Stamped onto documents as preparedBy / issuedBy / receivedBy, and consulted
//  before allowing an edit. Held here so a service can record who did something
//  without being handed the name at every call site.
class Session
{
public:
    QString currentUser() const { return m_currentUser; }
    void setCurrentUser(const QString &user) { m_currentUser = user; }

    QString userRole() const { return m_userRole; }
    void setUserRole(const QString &role) { m_userRole = role; }

    // Only an owner or an editor may change anything; every other role,
    // including an unrecognised one, is read-only.
    bool canEdit() const
    {
        return m_userRole == QLatin1String("owner")
            || m_userRole == QLatin1String("editor");
    }

private:
    QString m_currentUser = QStringLiteral("User");
    QString m_userRole = QStringLiteral("editor");
};

// -----------------------------------------------------------------------------
//  Company profile - our own details, printed on every document
// -----------------------------------------------------------------------------
QVariantMap loadCompanyProfile();
bool saveCompanyProfile(const QVariantMap &profile);

#endif // CORE_APPSETTINGS_H
