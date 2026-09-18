# Releasing Enstein Stock Manager

Windows binaries are produced by GitHub Actions (`.github/workflows/build.yml`).
There is no way to build a Windows `.exe` on a Linux machine here, so the
Windows job on the CI runner is what actually produces the shipping artifacts.

## Cutting a release

```bash
git tag v2.1.4          # must be v<major>.<minor>.<patch>
git push origin main
git push origin v2.1.4
```

Pushing the tag runs the build and attaches the artifacts to a GitHub Release.
To produce binaries without releasing, use **Actions -> Build Windows and Linux
(Qt) -> Run workflow**; the artifacts appear under the run's "Artifacts"
section instead.

The tag is the single source of truth for the version. It flows into the CMake
project version (`-DAPP_VERSION`), the version resource embedded in the `.exe`,
the installer filename, and `QCoreApplication::applicationVersion()`.

## What the Windows job produces

| Artifact | What it is | When to use it |
|---|---|---|
| `EnsteinStockManager-Setup-<version>.exe` | Inno Setup installer | Normal deployment. Start Menu entry, desktop icon, uninstaller, in-place upgrades. |
| `EnsteinStockManager-Portable-<version>.exe` | Single self-extracting exe | Copy one file to a machine and double-click. No install, no admin rights. |
| `EnsteinStockManager-Windows-<version>.zip` | The plain deployed folder | IT rollouts, or debugging what actually got bundled. |

Both `.exe` files are fully self-contained: the Qt runtime, the QML modules,
the SQL driver plugins, `libpq` with its OpenSSL dependencies, and the MSVC
runtime all travel inside them. The target machine needs nothing preinstalled.

### Portable vs installer

The portable exe unpacks itself into a temporary folder on every launch and
starts the app from there, so first paint takes a few seconds longer and the
extraction repeats each run. The installer unpacks once. Prefer the installer
for anything permanent; the portable build is for quick transfers and machines
where you cannot install software.

Neither build stores data next to the exe. The local SQLite fallback database
lives in `%APPDATA%\Enstein Robots and Automations Pvt Limited\Enstein Stock
Manager\`, so it survives upgrades and works from a read-only location.

## Guardrails in the pipeline

The Windows job fails rather than shipping a broken package if any of these
break:

- **Bare-PATH smoke test.** The deployed app is launched with `PATH` reduced to
  `System32`, so it can only load DLLs it actually ships with — the same
  situation as an end-user machine with no Qt installed. If a DLL, plugin or
  QML module is missing the process exits immediately and the job fails. This
  is the check that catches "works on the build machine, dies everywhere else".
- **`libpq` bundling.** Without `libpq.dll` the PostgreSQL driver fails to
  load, and the app silently falls back to a *local* SQLite file. Nothing
  looks broken — users just quietly stop sharing data. The job verifies both
  `sqldrivers\qsqlpsql.dll` and `libpq.dll` are present.
- **MSVC runtime bundling.** Verified explicitly, with a manual copy as a
  fallback if `windeployqt --compiler-runtime` finds nothing.

A Linux-only failure does not block the Windows release.

## Code signing

The binaries are unsigned, so Windows SmartScreen shows a
"Windows protected your PC" warning on first run; users click *More info ->
Run anyway*. The warning disappears once the exe is signed with an
Authenticode certificate (an OV certificate builds reputation over time; an EV
certificate is trusted immediately). To add signing later, sign
`dist\EnsteinStockManager.exe` before the installer and portable steps, then
sign the two resulting `.exe` files as well.
