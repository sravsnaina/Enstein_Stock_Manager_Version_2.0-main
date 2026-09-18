#ifndef SERVERSETUP_H
#define SERVERSETUP_H

#include <QObject>
#include <QString>
#include <QVariantMap>

// ==================== ServerSetup ====================
//
// One-click provisioning of a shared PostgreSQL server on the local
// network, so a single designated computer can become the "server" for
// every other installation of the app without anyone hand-installing or
// configuring PostgreSQL themselves.
//
// provisionAsServer() runs a single elevated helper script (bash + pkexec
// on Linux, an elevated PowerShell script on Windows) that:
//   - installs PostgreSQL if it isn't already present (via apt / winget),
//   - relies on it being registered as a persistent OS service so it
//     survives reboots (systemd on Linux, Windows Service on Windows —
//     both happen automatically as part of the normal package install),
//   - opens it up to LAN connections (listen_addresses + pg_hba.conf),
//   - creates a dedicated "stockmanager_app" login + "stockmanager"
//     database with a freshly generated password,
//   - opens the firewall port.
//
// On success, the generated host/port/user/password are handed back so
// the admin can enter them into every other machine's Database Connection
// dialog (the existing QPSQL client path in DatabaseManager). This class
// never touches SQL itself once provisioning is done — it only prepares
// the server so DatabaseManager::configureConnection() can connect to it
// like any other PostgreSQL server.
//
class ServerSetup : public QObject
{
    Q_OBJECT
public:
    explicit ServerSetup(QObject *parent = nullptr);

    // Best-guess LAN IPv4 address of this machine, for display to the admin
    // so they know what to type into other computers' connection dialogs.
    Q_INVOKABLE QString lanAddressHint() const;

    // True once provisionAsServer() has completed successfully on this
    // machine at least once.
    Q_INVOKABLE bool isServerProvisioned() const;

    // Runs the (asynchronous) provisioning flow. Emits progress() while
    // running, then finished() exactly once with the result.
    Q_INVOKABLE void provisionAsServer();

signals:
    void progress(const QString &message);
    void finished(bool success, const QVariantMap &result);

private:
    void runLinux();
    void runWindows();
    void runUnsupported();

    QString m_appPassword;
    QString m_adminPassword;   // Windows only: postgres superuser password
};

#endif // SERVERSETUP_H
