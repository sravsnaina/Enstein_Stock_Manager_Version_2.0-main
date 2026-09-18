#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <QStringList>
#include <QSqlDatabase>

// ==================== DatabaseManager ====================
//
// Central data store for the application. Replaces the previous
// "xlsx files in a Dropbox folder" storage with a real SQL database so
// that multiple users can work against one shared, consistent source of
// truth.
//
// The same code targets two backends via the standard Qt SQL drivers:
//   * PostgreSQL ("QPSQL") - the shared, multi-user production server.
//   * SQLite     ("QSQLITE") - a local single-file fallback for offline /
//                              single-machine use and for development.
//
// All SQL emitted here is written to be portable across both backends.
//
// Connection settings are read from QSettings("EnsteinRobotics",
// "StockManager") under the "database/" group:
//   database/driver   -> "QPSQL" or "QSQLITE"   (default: QSQLITE local file)
//   database/host, database/port, database/name,
//   database/user,   database/password           (server backends only)
//
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // ---- Connection ----
    // Opens the connection using the persisted settings (or a local SQLite 
    // fallback). Creates the schema and seeds the default admin user.
    bool connectDatabase();
    bool isConnected() const;
    bool isServerBackend() const;   // true when talking to a real server (Postgres/MySQL)
    QString backendName() const;    // e.g. "PostgreSQL (host)" or "SQLite (local)"
    QString lastError() const { return m_lastError; }

    // Persist new connection settings and reconnect. Returns true on success.
    bool configureConnection(const QString &driver,
                             const QString &host, int port,
                             const QString &name,
                             const QString &user, const QString &password);

    // Read the currently persisted settings (for populating a settings UI).
    QVariantMap connectionSettings() const;

    // One-time hand-off from single-machine storage to a shared server: copies
    // this computer's local SQLite contents into the server it is now connected
    // to, so the work already done here shows up on every other machine.
    // Tables that already hold rows on the server are left untouched, so a
    // second run (or running it on a client) can never duplicate shared data.
    // Sequence counters are raised rather than overwritten, so PO/GRN numbering
    // continues instead of restarting and colliding.
    // Returns {success, message, copied:{table:rows}, skipped:[tables]}.
    QVariantMap migrateLocalDataToServer();

    // ---- Generic table access (rows keyed by DB column name) ----
    QVector<QVariantMap> selectAll(const QString &table, const QString &orderBy = QString());

    // Delete every row then insert the supplied rows, atomically. Mirrors the
    // old "rewrite the whole sheet" semantics but transactionally.
    bool replaceAll(const QString &table, const QVector<QVariantMap> &rows);

    // Insert-or-update a single row by its primary-key column(s).
    bool upsert(const QString &table, const QStringList &keyCols, const QVariantMap &row);

    // Plain insert of a single row (for append-only tables such as the log).
    bool insert(const QString &table, const QVariantMap &row);

    bool removeRow(const QString &table, const QString &keyCol, const QVariant &keyVal);

    // ---- Live sync across machines ----
    // Every write bumps a single version row. Clients poll hasRemoteChanges()
    // (one cheap SELECT) and only reload when another machine has written, so a
    // change made anywhere shows up everywhere without hammering the server.
    // Our own writes advance the local mark too, so we never reload our own work.
    bool hasRemoteChanges();
    void syncVersionMark();
    int dataVersion();

    // ---- Shared sequence counters (atomic, multi-user safe) ----
    int nextCounter(const QString &name);   // returns current value, advances stored value
    int peekCounter(const QString &name);   // returns next value without advancing

    // ---- Users / authentication ----
    // Creates the user if it does not already exist (used to seed admin).
    bool ensureUser(const QString &username, const QString &plainPassword,
                    const QString &role, const QString &displayName);
    // Returns the user's role on success, empty string on failure.
    QString authenticate(const QString &username, const QString &plainPassword);

signals:
    void connectionChanged(bool connected);
    void databaseError(const QString &message);

private:
    QString localSqlitePath() const;
    int tableRowCount(const QString &table);
    bool createSchema();
    bool migrateSchema();
    bool tableHasColumn(const QString &table, const QString &column);
    void seedDefaults();
    bool isPostgres() const;
    QString hashPassword(const QString &plain) const;
    void setError(const QString &context, const QString &detail);

    QSqlDatabase database() const;

    QString m_connectionName;   // named Qt SQL connection
    QString m_driver;
    QString m_backendLabel;
    bool m_connected = false;
    bool m_serverBackend = false;                
    QString m_lastError;
    void bumpDataVersion();
    int m_localVersion = -1;   // data version as of our own last write or reload
};

#endif // DBMANAGER_H
