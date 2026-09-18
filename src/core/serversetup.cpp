#include "core/serversetup.h"

#include <QFile>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QRandomGenerator>
#include <QSettings>
#include <QSharedPointer>
#include <QStandardPaths>

namespace {

QString randomAlnum(int len)
{
    static const QString chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    QString s;
    s.reserve(len);
    for (int i = 0; i < len; ++i)
        s.append(chars.at(QRandomGenerator::global()->bounded(chars.size())));
    return s;
}

const QString kAppUser = "stockmanager_app";
const QString kAppDb = "stockmanager";

} // namespace

ServerSetup::ServerSetup(QObject *parent) : QObject(parent) {}

QString ServerSetup::lanAddressHint() const
{
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp)) continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress addr = entry.ip();
            if (addr.protocol() == QAbstractSocket::IPv4Protocol && !addr.isLoopback())
                return addr.toString();
        }
    }
    return QString();
}

bool ServerSetup::isServerProvisioned() const
{
    QSettings settings("EinsteinRobotics", "StockManager");
    return settings.value("database/serverProvisioned", false).toBool();
}

void ServerSetup::provisionAsServer()
{
    m_appPassword = randomAlnum(24);
    m_adminPassword = randomAlnum(24);
#if defined(Q_OS_LINUX)
    runLinux();
#elif defined(Q_OS_WIN)
    runWindows();
#else
    runUnsupported();
#endif
}

void ServerSetup::runUnsupported()
{
    emit finished(false, QVariantMap{
        {"message", "Automatic server setup is only available on Windows and Linux."}
    });
}

#if defined(Q_OS_LINUX)

void ServerSetup::runLinux()
{
    emit progress("Setting up the shared database (you'll be asked to enter your password)...");

    // A single elevated script, run once via pkexec, so the admin only sees
    // one authentication prompt for the whole flow. "set -e" means any
    // failing step aborts the rest and the exit code comes back non-zero.
    const QString script = QStringLiteral(
        "#!/bin/bash\n"
        "set -e\n"
        "APPUSER=\"%1\"\n"
        "APPDB=\"%2\"\n"
        "APPPASS=\"%3\"\n"
        "\n"
        "if ! command -v psql >/dev/null 2>&1; then\n"
        "    apt-get update -qq\n"
        "    apt-get install -y postgresql\n"
        "fi\n"
        "\n"
        "su postgres -c \"psql -v ON_ERROR_STOP=1 -c \\\"ALTER SYSTEM SET listen_addresses = '*';\\\"\"\n"
        "\n"
        "HBA_FILE=$(su postgres -c \"psql -tAc 'SHOW hba_file;'\" | tr -d '[:space:]')\n"
        "if ! grep -q \"$APPDB[[:space:]]*$APPUSER\" \"$HBA_FILE\" 2>/dev/null; then\n"
        "    {\n"
        "        echo \"host    $APPDB    $APPUSER    127.0.0.1/32       scram-sha-256\"\n"
        "        echo \"host    $APPDB    $APPUSER    ::1/128            scram-sha-256\"\n"
        "        echo \"host    $APPDB    $APPUSER    10.0.0.0/8         scram-sha-256\"\n"
        "        echo \"host    $APPDB    $APPUSER    172.16.0.0/12      scram-sha-256\"\n"
        "        echo \"host    $APPDB    $APPUSER    192.168.0.0/16     scram-sha-256\"\n"
        "    } >> \"$HBA_FILE\"\n"
        "fi\n"
        "\n"
        "systemctl enable postgresql\n"
        "systemctl restart postgresql\n"
        "\n"
        "if ! su postgres -c \"psql -tAc \\\"SELECT 1 FROM pg_roles WHERE rolname='$APPUSER'\\\"\" | grep -q 1; then\n"
        "    su postgres -c \"psql -v ON_ERROR_STOP=1 -c \\\"CREATE ROLE $APPUSER LOGIN PASSWORD '$APPPASS';\\\"\"\n"
        "else\n"
        "    su postgres -c \"psql -v ON_ERROR_STOP=1 -c \\\"ALTER ROLE $APPUSER WITH PASSWORD '$APPPASS';\\\"\"\n"
        "fi\n"
        "\n"
        "if ! su postgres -c \"psql -tAc \\\"SELECT 1 FROM pg_database WHERE datname='$APPDB'\\\"\" | grep -q 1; then\n"
        "    su postgres -c \"psql -v ON_ERROR_STOP=1 -c \\\"CREATE DATABASE $APPDB OWNER $APPUSER;\\\"\"\n"
        "else\n"
        "    # The database already exists (e.g. the app created it earlier under a\n"
        "    # different login, such as OS-user peer auth). Transfer full ownership\n"
        "    # of the database AND everything in it to the app role so it can\n"
        "    # read/write. Non-destructive: data is preserved, only owners change.\n"
        "    OLDOWNER=$(su postgres -c \"psql -tAc \\\"SELECT pg_get_userbyid(datdba) FROM pg_database WHERE datname='$APPDB'\\\"\" | tr -d '[:space:]')\n"
        "    su postgres -c \"psql -v ON_ERROR_STOP=1 -c \\\"ALTER DATABASE $APPDB OWNER TO $APPUSER;\\\"\"\n"
        "    if [ -n \"$OLDOWNER\" ] && [ \"$OLDOWNER\" != \"$APPUSER\" ] && [ \"$OLDOWNER\" != \"postgres\" ]; then\n"
        "        su postgres -c \"psql -v ON_ERROR_STOP=1 -d $APPDB -c \\\"REASSIGN OWNED BY $OLDOWNER TO $APPUSER;\\\"\"\n"
        "    fi\n"
        "fi\n"
        "\n"
        "if command -v ufw >/dev/null 2>&1; then\n"
        "    ufw allow 5432/tcp || true\n"
        "fi\n"
        "\n"
        "echo \"PROVISION_OK\"\n"
    ).arg(kAppUser, kAppDb, m_appPassword);

    const QString scriptPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                                + "/enstein_provision_server.sh";
    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit finished(false, QVariantMap{{"message", "Could not write the setup script."}});
        return;
    }
    file.write(script.toUtf8());
    file.close();
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

    auto *proc = new QProcess(this);
    proc->setProgram("pkexec");
    proc->setArguments({"bash", scriptPath});

    // pkexec only ever emits one of finished()/errorOccurred(FailedToStart)
    // for a given run, but guard against double handling all the same.
    auto handled = QSharedPointer<bool>::create(false);

    connect(proc, &QProcess::finished, this,
            [this, proc, scriptPath, handled](int exitCode, QProcess::ExitStatus status) {
                if (*handled) return;
                *handled = true;

                const QString out = QString::fromUtf8(proc->readAllStandardOutput());
                const QString err = QString::fromUtf8(proc->readAllStandardError());
                QFile::remove(scriptPath);
                proc->deleteLater();

                const bool ok = (status == QProcess::NormalExit && exitCode == 0
                                 && out.contains("PROVISION_OK"));
                if (!ok) {
                    emit finished(false, QVariantMap{
                        {"message", err.trimmed().isEmpty() ? "Setup failed." : err.trimmed()}
                    });
                    return;
                }

                QSettings settings("EinsteinRobotics", "StockManager");
                settings.setValue("database/serverProvisioned", true);
                settings.sync();

                emit finished(true, QVariantMap{
                    {"host", lanAddressHint()},
                    {"port", 5432},
                    {"name", kAppDb},
                    {"user", kAppUser},
                    {"password", m_appPassword},
                    {"message", "Shared database ready."}
                });
            });

    connect(proc, &QProcess::errorOccurred, this,
            [proc, scriptPath, handled, this](QProcess::ProcessError) {
                if (*handled) return;
                if (proc->state() != QProcess::NotRunning) return;
                *handled = true;
                QFile::remove(scriptPath);
                proc->deleteLater();
                emit finished(false, QVariantMap{
                    {"message", "Could not run the setup helper (is 'pkexec' installed?)."}
                });
            });

    proc->start();
}

void ServerSetup::runWindows() {}

#elif defined(Q_OS_WIN)

void ServerSetup::runLinux() {}

void ServerSetup::runWindows()
{
    // NOTE: the installer arguments below were validated against EDB's
    // published PostgreSQL command-line-parameters documentation (unattended
    // mode, --superpassword, --serverport default 5432, and the critical
    // --disable-components stackbuilder so the silent install doesn't hang).
    // The flow has NOT yet been exercised on a real Windows machine, so run
    // it once on real hardware before shipping — see the manual test checklist
    // in the project notes.
    emit progress("Setting up the shared database (a Windows security prompt will appear)...");

    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString scriptPath = tempDir + "/enstein_provision_server.ps1";
    const QString resultPath = tempDir + "/enstein_provision_result.txt";

    QFile::remove(resultPath);

    const QString script = QStringLiteral(
        "$ErrorActionPreference = \"Stop\"\n"
        "$AppUser = \"%1\"\n"
        "$AppDb = \"%2\"\n"
        "$AppPass = \"%3\"\n"
        "$AdminPass = \"%4\"\n"
        "$ResultPath = \"%5\"\n"
        "try {\n"
        "    function Find-PgRoot {\n"
        "        Get-ChildItem \"C:\\Program Files\\PostgreSQL\" -ErrorAction SilentlyContinue |\n"
        "            Sort-Object Name -Descending | Select-Object -First 1\n"
        "    }\n"
        "    # --disable-components stackbuilder is essential: without it the\n"
        "    # installer launches Stack Builder at the end and the unattended run\n"
        "    # hangs waiting for interaction. --unattendedmodeui none keeps it fully\n"
        "    # silent. Standard PostgreSQL defaults the port to 5432.\n"
        "    $InstArgs = \"--mode unattended --unattendedmodeui none --disable-components stackbuilder --superpassword $AdminPass --serverport 5432\"\n"
        "\n"
        "    $pgRoot = Find-PgRoot\n"
        "    if (-not $pgRoot) {\n"
        "        $installed = $false\n"
        "        # Preferred path: winget (present on modern Windows 10/11).\n"
        "        if (Get-Command winget -ErrorAction SilentlyContinue) {\n"
        "            try {\n"
        "                winget install --id PostgreSQL.PostgreSQL -e --silent `\n"
        "                    --accept-package-agreements --accept-source-agreements `\n"
        "                    --override \"$InstArgs\"\n"
        "                Start-Sleep -Seconds 5\n"
        "                if (Find-PgRoot) { $installed = $true }\n"
        "            } catch { }\n"
        "        }\n"
        "        # Fallback for machines without winget (older Windows 10, Windows\n"
        "        # Server, locked-down images): download EDB's official installer\n"
        "        # directly and run it with the same unattended flags.\n"
        "        if (-not $installed) {\n"
        "            $url = \"https://get.enterprisedb.com/postgresql/postgresql-16.8-1-windows-x64.exe\"\n"
        "            $exe = Join-Path $env:TEMP \"enstein_pg_installer.exe\"\n"
        "            [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12\n"
        "            Invoke-WebRequest -Uri $url -OutFile $exe -UseBasicParsing\n"
        "            Start-Process -FilePath $exe -ArgumentList $InstArgs -Wait\n"
        "            Start-Sleep -Seconds 5\n"
        "            Remove-Item $exe -ErrorAction SilentlyContinue\n"
        "        }\n"
        "        $pgRoot = Find-PgRoot\n"
        "    }\n"
        "    if (-not $pgRoot) { throw \"PostgreSQL install could not be found after installation.\" }\n"
        "    $psql = Join-Path $pgRoot.FullName \"bin\\psql.exe\"\n"
        "    $env:PGPASSWORD = $AdminPass\n"
        "\n"
        "    # The service can take a few seconds to finish starting after a\n"
        "    # fresh install; wait for it before issuing any psql commands.\n"
        "    $svc = $null\n"
        "    for ($i = 0; $i -lt 30; $i++) {\n"
        "        $svc = Get-Service -Name \"postgresql-x64-*\" -ErrorAction SilentlyContinue | Select-Object -First 1\n"
        "        if ($svc -and $svc.Status -eq 'Running') { break }\n"
        "        if ($svc) { Start-Service -Name $svc.Name -ErrorAction SilentlyContinue }\n"
        "        Start-Sleep -Seconds 2\n"
        "    }\n"
        "\n"
        "    & $psql -U postgres -c \"ALTER SYSTEM SET listen_addresses = '*';\" | Out-Null\n"
        "\n"
        "    $hbaFile = (& $psql -U postgres -tA -c \"SHOW hba_file;\").Trim()\n"
        "    $needsRule = -not (Select-String -Path $hbaFile -Pattern \"$AppDb.*$AppUser\" -Quiet -ErrorAction SilentlyContinue)\n"
        "    if ($needsRule) {\n"
        "        Add-Content -Path $hbaFile -Value \"host    $AppDb    $AppUser    10.0.0.0/8         scram-sha-256\"\n"
        "        Add-Content -Path $hbaFile -Value \"host    $AppDb    $AppUser    172.16.0.0/12      scram-sha-256\"\n"
        "        Add-Content -Path $hbaFile -Value \"host    $AppDb    $AppUser    192.168.0.0/16     scram-sha-256\"\n"
        "    }\n"
        "\n"
        "    $svc = Get-Service -Name \"postgresql-x64-*\" -ErrorAction SilentlyContinue | Select-Object -First 1\n"
        "    if ($svc) {\n"
        "        Set-Service -Name $svc.Name -StartupType Automatic\n"
        "        Restart-Service -Name $svc.Name -Force\n"
        "    }\n"
        "\n"
        "    $roleCheck = (& $psql -U postgres -tA -c \"SELECT 1 FROM pg_roles WHERE rolname='$AppUser'\").Trim()\n"
        "    if ($roleCheck -eq \"1\") {\n"
        "        & $psql -U postgres -c \"ALTER ROLE $AppUser WITH PASSWORD '$AppPass';\" | Out-Null\n"
        "    } else {\n"
        "        & $psql -U postgres -c \"CREATE ROLE $AppUser LOGIN PASSWORD '$AppPass';\" | Out-Null\n"
        "    }\n"
        "\n"
        "    $dbCheck = (& $psql -U postgres -tA -c \"SELECT 1 FROM pg_database WHERE datname='$AppDb'\").Trim()\n"
        "    if ($dbCheck -ne \"1\") {\n"
        "        & $psql -U postgres -c \"CREATE DATABASE $AppDb OWNER $AppUser;\" | Out-Null\n"
        "    }\n"
        "\n"
        "    New-NetFirewallRule -DisplayName \"Enstein Stock Manager DB\" -Direction Inbound `\n"
        "        -Protocol TCP -LocalPort 5432 -Action Allow -ErrorAction SilentlyContinue | Out-Null\n"
        "\n"
        "    \"PROVISION_OK\" | Out-File -FilePath $ResultPath -Encoding utf8\n"
        "} catch {\n"
        "    $_.Exception.Message | Out-File -FilePath $ResultPath -Encoding utf8\n"
        "}\n"
    ).arg(kAppUser, kAppDb, m_appPassword, m_adminPassword, resultPath);

    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit finished(false, QVariantMap{{"message", "Could not write the setup script."}});
        return;
    }
    file.write(script.toUtf8());
    file.close();

    // Elevated child can't pipe stdout back to us across the UAC boundary,
    // so the script writes its own result file, and we just wait for the
    // (non-elevated) launcher process to return before reading it.
    const QString innerArgs = QStringLiteral("-NoProfile -ExecutionPolicy Bypass -File \"%1\"").arg(scriptPath);
    const QString launchCmd = QStringLiteral(
        "Start-Process -FilePath 'powershell.exe' -ArgumentList '%1' -Verb RunAs -Wait").arg(innerArgs);

    auto *proc = new QProcess(this);
    proc->setProgram("powershell.exe");
    proc->setArguments({"-NoProfile", "-Command", launchCmd});

    auto handled = QSharedPointer<bool>::create(false);

    auto finishFromResultFile = [this, scriptPath, resultPath, handled]() {
        if (*handled) return;
        *handled = true;

        QFile result(resultPath);
        QString text;
        if (result.open(QIODevice::ReadOnly | QIODevice::Text))
            text = QString::fromUtf8(result.readAll()).trimmed();

        QFile::remove(scriptPath);
        QFile::remove(resultPath);

        if (text != "PROVISION_OK") {
            emit finished(false, QVariantMap{
                {"message", text.isEmpty() ? "Setup failed or was cancelled." : text}
            });
            return;
        }

        QSettings settings("EinsteinRobotics", "StockManager");
        settings.setValue("database/serverProvisioned", true);
        settings.sync();

        emit finished(true, QVariantMap{
            {"host", lanAddressHint()},
            {"port", 5432},
            {"name", kAppDb},
            {"user", kAppUser},
            {"password", m_appPassword},
            {"message", "Shared database ready."}
        });
    };

    connect(proc, &QProcess::finished, this,
            [proc, finishFromResultFile](int, QProcess::ExitStatus) {
                finishFromResultFile();
                proc->deleteLater();
            });
    connect(proc, &QProcess::errorOccurred, this,
            [proc, handled, this](QProcess::ProcessError) {
                if (*handled) return;
                if (proc->state() != QProcess::NotRunning) return;
                *handled = true;
                proc->deleteLater();
                emit finished(false, QVariantMap{
                    {"message", "Could not run the setup helper."}
                });
            });

    proc->start();
}

#else

void ServerSetup::runLinux() {}
void ServerSetup::runWindows() {}

#endif
