# Windows Server-Provisioning — Manual Test Checklist

The Linux server-setup path has been built and run. The **Windows** path
(`ServerSetup::runWindows()` in `serversetup.cpp`) has been written to match
EDB's published PostgreSQL installer parameters, but has **not** been run on a
real Windows machine. This is the one-time manual test to do before relying on
it in production.

You need **two Windows machines on the same LAN** (or one Windows machine +
one Linux/other machine): one acts as the server, the other as a client.

## A. Server machine

1. Install the app from the Windows ZIP (or run a local build).
2. Toolbar → open **Database Connection**.
3. Click **"Set up this computer as the server"**.
4. Approve the Windows UAC prompt when it appears.
5. Wait. First run installs PostgreSQL silently — via `winget` if present,
   otherwise by downloading EDB's official installer directly (~344 MB, so
   this can take several minutes on the first machine). The button shows
   "Setting up..." throughout.

> **No winget? Still fine.** On machines without `winget` (older Windows 10,
> Windows Server, locked-down corporate images) the script automatically
> falls back to downloading the EDB installer. It only needs outbound
> internet access to `get.enterprisedb.com` for that first install.

**Expected on success:** a green box appears listing **Host / Port / Database /
User / Password**. Write these down — the password is shown only once.

### If it fails, capture the reason
The dialog shows the failure message. Common causes and checks:

| Symptom | Check on the server machine |
|---|---|
| "Could not run the setup helper" | UAC was likely declined. Re-run and approve the prompt. |
| "PostgreSQL install could not be found" | Both install paths failed. Check outbound access to `get.enterprisedb.com`; check `C:\Program Files\PostgreSQL\`. Try the manual install line below. |
| Hangs forever | Stack Builder dialog — confirm the installer args include `--disable-components stackbuilder`. |
| Setup ok but can't connect | Service running? `Get-Service postgresql-x64-*`. Port open? `Test-NetConnection localhost -Port 5432`. |

### Manual reproduction of the winget step (for debugging)
Run in an **elevated** PowerShell:
```powershell
winget install --id PostgreSQL.PostgreSQL -e --silent `
  --accept-package-agreements --accept-source-agreements `
  --override "--mode unattended --unattendedmodeui none --disable-components stackbuilder --superpassword TestPass123 --serverport 5432"
```
If this line fails, the installer-argument format is what needs adjusting —
that is the single most likely thing to differ across PostgreSQL versions.

### Verify the server is reachable on the LAN
From the server machine:
```powershell
Get-Service postgresql-x64-*                 # should be Running, StartType Automatic
Test-NetConnection <server-LAN-IP> -Port 5432  # should say TcpTestSucceeded : True
```

## B. Client machine

1. Install + launch the app.
2. **Database Connection** → choose **PostgreSQL (connect to shared server)**.
3. Enter the Host (server's LAN IP), Port, Database, User, Password from step A.
4. **Connect & Save**.

**Expected:** status bar says "Connected to shared database". If instead you see
**"Could NOT connect to PostgreSQL — using a LOCAL file…"**, the client did not
reach the server — read the reason shown and check firewall / IP / credentials.
This warning is deliberate: it means the client would otherwise silently use its
own local file, so it must never be ignored.

## C. Prove multi-device sync

1. On the **client**, add a vendor / stock item.
2. On the **server** app, reopen the relevant screen (or use its refresh).
3. The new record must appear on the server too — and vice versa.

If C works both directions, the end-to-end multi-device flow is confirmed.

## Driver-bundling note
On a **packaged** build, connecting to PostgreSQL only works if `libpq.dll` +
the QPSQL plugin are bundled. The CI workflow (`.github/workflows/build.yml`)
does this and fails the build if they're missing. If a client shows the
"using a LOCAL file" warning with a driver-not-available reason, the package is
missing `libpq.dll` — rebuild via CI rather than copying the bare `.exe`.
