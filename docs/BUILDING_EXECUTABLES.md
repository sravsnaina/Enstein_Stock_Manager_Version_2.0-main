# Building Enstein Stock Manager into an Executable

How to turn this source tree into something a user can double-click: a Windows
`.exe` and a single-file Ubuntu/Linux executable (AppImage).

This document is the authoritative build-and-package reference.
[docs/RELEASING.md](RELEASING.md) covers the *release* process (tagging,
GitHub Releases, code signing) and defers to this file for the mechanics.

---

## 1. What gets produced

| Platform | Artifact | Built by | Needs Qt on the target machine? |
|---|---|---|---|
| Windows | `EnsteinStockManager-Setup-<version>.exe` | `packaging/windows/installer.iss` (Inno Setup 6) | No |
| Windows | `EnsteinStockManager-Portable-<version>.exe` | `packaging/windows/portable.nsi` (NSIS 3) | No |
| Windows | `EnsteinStockManager-Windows-<version>.zip` | zip of the `windeployqt` output folder | No |
| Ubuntu / Linux | `Enstein_Stock_Manager-x86_64.AppImage` | `packaging/linux/create_appimage.sh`, or the CI Linux job | No |
| Any | `build/EnsteinStockManager` | `cmake --build build` | **Yes** — development binary only |

The plain `build/EnsteinStockManager` is *not* shippable: it resolves the Qt
libraries and Qt's own QML modules from the machine it was built on. Only the
installer, the portable `.exe` and the AppImage are self-contained.

**The application's own QML is already inside the binary.** `qml/CMakeLists.txt`
declares the frontend with `qt_add_qml_module(... STATIC)`, so every `.qml`
file in `QML_FILES` is compiled into the executable and loaded from
`qrc:/Enstein/Main.qml`. Packaging never has to copy the project's `.qml`
files — but it must still bundle **Qt's** QML modules (QtQuick, QtQuick.Controls,
QtQuick.Layouts, …), which is what `windeployqt --qmldir` and
`linuxdeploy-plugin-qt` are for.

### No cross-compiling

A Windows `.exe` cannot be produced from Linux in this project. The Windows
build needs MSVC, `windeployqt`, Inno Setup and NSIS. Build it on a Windows
machine (section 5) or let the CI Windows runner do it (section 7).

---

## 2. Prerequisites

### Shared

- CMake **3.16+** (`cmake_minimum_required` in the top-level `CMakeLists.txt`)
- A C++17 compiler
- Qt **6.5+** with the `Core Gui Qml Quick Network Widgets Sql QuickControls2`
  modules. CI pins **6.10.1**; keeping your local Qt close to that is the
  cheapest way to avoid "works here, not in CI".
- QXlsx is **vendored** at `third_party/QXlsx` — nothing to clone or install.
  A missing/empty folder means an incomplete checkout, and CMake stops with a
  `FATAL_ERROR` saying so.

### Ubuntu / Debian

Distro Qt (simplest, good enough for development):

```bash
sudo apt update
sudo apt install -y \
    cmake build-essential \
    qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
    libqt6sql6-sqlite libqt6sql6-psql \
    libpq5 libxcb-cursor0
```

Two of those are easy to miss and cause silent, hard-to-diagnose behaviour:

- **`libqt6sql6-psql` + `libpq5`** — the Qt `QPSQL` driver plugin and the
  PostgreSQL client library it links against. Without them the app cannot
  reach the shared server and falls back to a *local* SQLite file. Nothing
  looks broken; users just quietly stop sharing data.
- **`libqt6sql6-sqlite`** — the fallback driver itself.

Newer Qt than the distro ships? Install Qt from the official online installer
and point CMake at it:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10.1/gcc_64
```

For **building AppImages** you additionally need `wget`, `file` and — to *run*
an AppImage on Ubuntu 22.04 or newer — `libfuse2`:

```bash
sudo apt install -y wget file libfuse2
```

### Windows

- Qt 6.10.1 for **MSVC 2022 64-bit** (`win64_msvc2022_64`), with Qt Quick and
  Qt Sql selected in the Qt installer
- Visual Studio 2022 (or Build Tools for VS 2022) with the **Desktop C++**
  workload — MSVC x64
- CMake 3.16+ and Ninja (both ship with the VS installer)
- [Inno Setup 6](https://jrsoftware.org/isdl.php) — builds the installer
- [NSIS 3](https://nsis.sourceforge.io/Download) — builds the portable `.exe`
- PostgreSQL for Windows (any recent version) — only for its `bin\libpq.dll`
  and OpenSSL DLLs, which get copied into the package

### macOS

```bash
brew install qt@6 cmake
```

macOS produces a `.app` bundle (`MACOSX_BUNDLE` is set), but there is no
packaging script and no CI job for it. It is a development target only.

---

## 3. Build from source (all platforms)

```bash
# Configure. On Windows/macOS, or with a non-system Qt, add:
#   -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.1/<compiler>
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build -j"$(nproc)"

# Run
./build/EnsteinStockManager
```

Frontend-only check (no run needed):

```bash
cmake --build build --target all_qmllint
```

Always ship **Release** builds. A Debug build of a Qt Quick application is
noticeably slower and links Qt's debug libraries, which the deployment tools
then have to carry.

---

## 4. Ubuntu: make a single-file executable (AppImage)

An AppImage is one file that carries the app, the Qt runtime, Qt's QML modules
and the SQL drivers. The user runs `chmod +x` once and double-clicks it. No
install, no root, no Qt on the machine.

### Which glibc, which Ubuntu

An AppImage runs on the distro it was built on **and newer**, never older —
glibc is not forward compatible. CI builds on **ubuntu-22.04**, so the
published AppImage covers Ubuntu 22.04, 24.04 and later. If you must support
20.04, build on 20.04 (a container is fine).

### Route A — the script in this repo

```bash
chmod +x packaging/linux/create_appimage.sh
./packaging/linux/create_appimage.sh
```

It configures and builds Release, runs `cmake --install` into `AppDir/usr`,
copies `packaging/linux/EnsteinStockManager.desktop` and
`assets/app-icon.png` into place, then calls **`linuxdeployqt`** to bundle Qt
and emit the AppImage.

You must supply `linuxdeployqt` yourself — the script does not download it:

```bash
wget -O packaging/linux/linuxdeployqt-x86_64.AppImage \
  https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x packaging/linux/linuxdeployqt-x86_64.AppImage
```

The script looks for `linuxdeployqt` in `PATH` first, then for
`packaging/linux/linuxdeployqt-x86_64.AppImage`, and exits if it finds
neither. It also needs `qmake` on `PATH` (or at
`/opt/Qt/6.11.1/gcc_64/bin/qmake`) so `linuxdeployqt` can locate the Qt
installation — add your Qt `bin/` directory to `PATH` before running:

```bash
export PATH=$HOME/Qt/6.10.1/gcc_64/bin:$PATH
```

### Route B — linuxdeploy + the Qt plugin (what CI does)

This is the route the release AppImage is actually built with
(`.github/workflows/build.yml`, job *Linux AppImage*). Prefer it when Route A
gives you trouble: `linuxdeploy-plugin-qt` tracks Qt 6 more closely than
`linuxdeployqt` does.

```bash
# 1. Build Release
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

# 2. Lay out the AppDir
mkdir -p AppDir/usr/bin \
         AppDir/usr/share/applications \
         AppDir/usr/share/icons/hicolor/256x256/apps
cp build/EnsteinStockManager AppDir/usr/bin/
cp packaging/linux/EnsteinStockManager.desktop AppDir/usr/share/applications/
cp assets/app-icon.png \
   AppDir/usr/share/icons/hicolor/256x256/apps/EnsteinStockManager.png

# 3. Fetch the tools
wget -q https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -q https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy*.AppImage

# 4. Bundle and build
export PATH=$HOME/Qt/6.10.1/gcc_64/bin:$PATH   # so the plugin finds qmake
export QML_SOURCES_PATHS=$PWD                  # scan this tree for QML imports
export EXTRA_QT_PLUGINS=sqldrivers             # see the warning below
export VERSION=2.1.4                           # puts the version in the filename
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
```

> **`EXTRA_QT_PLUGINS=sqldrivers` is not optional.**
> `linuxdeploy-plugin-qt` does not bundle the SQL drivers by default. Leave it
> out and `libqsqlpsql.so` is missing from the AppImage, the PostgreSQL
> connection fails on every end-user machine, and the app falls back to a
> local SQLite file *without any visible error*. `libpq.so.5` is then pulled
> in automatically, provided `libpq5` is installed on the build machine.

The icon and desktop-entry names must agree, or `linuxdeploy` refuses to
build: `Icon=EnsteinStockManager` in the `.desktop` file matches
`EnsteinStockManager.png` in the icons directory. (The CI job writes its own
equivalent `.desktop` file inline using the name `enstein-stock`; either
naming is fine as long as it is self-consistent.)

### Output filename

`linuxdeploy` derives the name from the `Name=` field of the `.desktop` file,
so `Name=Enstein Stock Manager` yields:

```
Enstein_Stock_Manager-x86_64.AppImage             # VERSION unset
Enstein_Stock_Manager-2.1.4-x86_64.AppImage       # VERSION=2.1.4
```

CI does not set `VERSION`, so the released file carries no version in its
name. Set it for local builds — it saves confusion when several are lying
around.

### Verify the AppImage before you hand it out

```bash
APPIMAGE=$(ls -1 Enstein_Stock_Manager-*.AppImage | head -n1)
chmod +x "$APPIMAGE"

# 1. It is a valid AppImage
"./$APPIMAGE" --appimage-version

# 2. The SQL driver and its client library really are inside
"./$APPIMAGE" --appimage-extract >/dev/null
find squashfs-root -name 'libqsqlpsql.so' -o -name 'libpq.so*' | sort
rm -rf squashfs-root

# 3. It actually starts (headless machines: force software rendering)
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "./$APPIMAGE"
```

Step 2 is the check CI enforces; the job fails rather than publish an AppImage
that would silently stop syncing. Run the real test too: copy the AppImage to
a machine with **no Qt installed** and launch it.

### Installing it on a user's machine

```bash
chmod +x Enstein_Stock_Manager-2.1.4-x86_64.AppImage

# Run it from anywhere
./Enstein_Stock_Manager-2.1.4-x86_64.AppImage

# Or make it a permanent, menu-visible install
sudo install -m 755 Enstein_Stock_Manager-2.1.4-x86_64.AppImage \
     /opt/EnsteinStockManager.AppImage
sudo install -m 644 packaging/linux/EnsteinStockManager.desktop \
     /usr/share/applications/
sudo install -m 644 assets/app-icon.png \
     /usr/share/icons/hicolor/256x256/apps/EnsteinStockManager.png
sudo sed -i 's|^Exec=.*|Exec=/opt/EnsteinStockManager.AppImage|' \
     /usr/share/applications/EnsteinStockManager.desktop
sudo update-desktop-database /usr/share/applications
```

Upgrading is replacing the one file. Application data lives elsewhere
(section 8), so it survives.

---

## 5. Windows: make the `.exe`

Run all of this from an **x64 Native Tools Command Prompt for VS 2022**, from
the repository root, so MSVC is on `PATH`. Commands are shown for PowerShell;
`$Env:PATH` already includes Qt's `bin` if you launched from the Qt-provided
prompt, otherwise add it.

### 5.1 Configure and build

```powershell
$Version = "2.1.4"     # must be plain x.y.z
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DAPP_VERSION="$Version"
cmake --build build
```

On Windows the build also compiles `packaging/windows/app.rc.in` (generated
into the build tree as `app.rc`), which embeds the application icon and the
file/product version metadata into the `.exe`. `WIN32_EXECUTABLE TRUE` keeps a
console window from opening alongside the UI.

### 5.2 Stage the app with its Qt runtime

```powershell
New-Item -ItemType Directory -Force -Path dist | Out-Null
Copy-Item build\EnsteinStockManager.exe dist\ -Force
windeployqt --release --qmldir . --sql --compiler-runtime dist\EnsteinStockManager.exe
```

Each flag earns its place:

| Flag | Why |
|---|---|
| `--qmldir .` | scans the tree for QML imports so Qt's own QML modules are copied in |
| `--sql` | copies the SQL driver plugins (`sqldrivers\qsqlpsql.dll`, `qsqlite.dll`) |
| `--compiler-runtime` | copies the MSVC runtime DLLs, so the app runs where no VC++ redistributable is installed |

`dist\` is now the complete application folder — this is what both packagers
consume, and what the ZIP artifact contains.

### 5.3 Add the PostgreSQL client libraries

`windeployqt` copies the Qt `QPSQL` *plugin* but **not** `libpq.dll`, which
the plugin loads at runtime. Copy it and its OpenSSL dependencies out of a
PostgreSQL installation:

```powershell
$pg  = Get-ChildItem 'C:\Program Files\PostgreSQL' -Directory |
       Sort-Object Name -Descending | Select-Object -First 1
$bin = Join-Path $pg.FullName 'bin'
Copy-Item (Join-Path $bin 'libpq.dll') dist\ -Force
foreach ($pat in 'libcrypto*.dll','libssl*.dll','libintl*.dll','libiconv*.dll','libwinpthread*.dll') {
    Get-ChildItem (Join-Path $bin $pat) -ErrorAction SilentlyContinue |
        ForEach-Object { Copy-Item $_.FullName dist\ -Force }
}
```

Then confirm the three files that must be present, or the package is broken:

```powershell
Test-Path dist\sqldrivers\qsqlpsql.dll   # PostgreSQL driver plugin
Test-Path dist\libpq.dll                 # its client library
Test-Path dist\vcruntime140.dll          # MSVC runtime
```

If `vcruntime140.dll` is missing, `--compiler-runtime` could not find the VC
redist directory; copy the DLLs from
`$Env:VCToolsRedistDir\x64\Microsoft.VC143.CRT\` into `dist\` by hand. CI does
exactly this as a fallback.

### 5.4 Smoke-test the staged folder

Do this **before** packaging. It is the single most valuable check in the
pipeline: the app is launched with `PATH` cut back to `System32`, so it can
only load DLLs it actually ships with — the same situation as a fresh end-user
machine with no Qt installed.

```powershell
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_QPA_PLATFORM  = 'offscreen'     # omit on a desktop; needed when headless
$env:QT_QUICK_BACKEND = 'software'
$p = Start-Process -FilePath (Resolve-Path .\dist\EnsteinStockManager.exe).Path `
                   -WorkingDirectory (Join-Path $PWD 'dist') -PassThru
Start-Sleep -Seconds 30
if ($p.HasExited) { throw "App exited early ($($p.ExitCode)) - the bundle is incomplete" }
Stop-Process -Id $p.Id -Force
```

A missing DLL, SQL plugin or Qt QML module makes the process die immediately.
This is the check that catches "works on the build machine, dies everywhere
else".

### 5.5 Build the installer (Inno Setup)

```powershell
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' `
    "/DAppVersion=$Version" "/DStageDir=$PWD\dist" "/DSourceRoot=$PWD" "/O$PWD" `
    packaging\windows\installer.iss
# -> EnsteinStockManager-Setup-2.1.4.exe
```

All three `/D` definitions are required — `installer.iss` raises `#error` if
`StageDir` or `SourceRoot` is missing. `StageDir` must be the `windeployqt`
output folder from 5.2/5.3.

The installer is x64-only (it refuses 32-bit Windows rather than failing later
with a cryptic DLL error), installs per-user without admin rights and
per-machine with them, and upgrades in place. Its `AppId` GUID is what makes
upgrades replace rather than duplicate the install — **never change that GUID
once a version has shipped**.

### 5.6 Build the portable single-file `.exe` (NSIS)

```powershell
& 'C:\Program Files (x86)\NSIS\makensis.exe' `
    "/DAPPVERSION=$Version" "/DSTAGEDIR=$PWD\dist" `
    "/DICONFILE=$PWD\assets\app-icon.ico" `
    "/DOUTFILE=$PWD\EnsteinStockManager-Portable-$Version.exe" `
    packaging\windows\portable.nsi
```

The portable build unpacks `dist\` into an NSIS-managed temp directory
(`$PLUGINSDIR`) on every launch, runs the app with `ExecWait`, and deletes the
temp directory when the app closes. So: nothing left behind, no admin rights,
but first paint takes a few seconds longer and the unpacking repeats every
run. Prefer the installer for anything permanent.

### 5.7 Build the ZIP

```powershell
Compress-Archive -Path dist\* -DestinationPath "EnsteinStockManager-Windows-$Version.zip"
```

For IT rollouts, and for inspecting what actually got bundled.

---

## 6. Versioning

`APP_VERSION` is the single source of truth and must be plain `x.y.z`. It
flows into:

- the CMake project version (`-DAPP_VERSION=x.y.z`, default `2.1.4` in the
  top-level `CMakeLists.txt`)
- the `VERSIONINFO` resource embedded in the `.exe` (via `app.rc.in`)
- the installer and portable filenames
- `APP_VERSION_STR`, which `main.cpp` passes to
  `QCoreApplication::setApplicationVersion()`

In CI the git tag supplies it, so those four can never drift apart. Locally,
pass `-DAPP_VERSION` when you configure; omit it and you get the cached
default, which will not match the tag you are pretending to build.

---

## 7. Letting CI build it (recommended for releases)

`.github/workflows/build.yml` builds everything above on real Windows and
Ubuntu runners, verifies the packages, and attaches them to a GitHub Release.

```bash
git tag v2.1.4            # must be v<major>.<minor>.<patch>
git push origin main
git push origin v2.1.4
```

Artifacts without a release: **Actions → Build Windows and Linux (Qt) → Run
workflow**. They appear under the run's *Artifacts* section.

The pipeline fails rather than shipping a broken package if the bare-`PATH`
smoke test dies, if `libpq.dll`/`qsqlpsql.dll` are missing on Windows, if the
MSVC runtime is absent, or if `libqsqlpsql.so`/`libpq.so*` are missing from
the AppImage. A Linux-only failure does not block the Windows release.

See [docs/RELEASING.md](RELEASING.md) for release notes, artifact choice and
code signing.

---

## 8. Where the packaged app keeps its data

None of it lives next to the executable, so upgrades, read-only install
directories and the portable build all work.

| What | Windows | Ubuntu / Linux |
|---|---|---|
| Local SQLite fallback database | `%APPDATA%\Enstein Robots and Automations Pvt Limited\Enstein Stock Manager\stockmanager.db` | `~/.local/share/Enstein Robots and Automations Pvt Limited/Enstein Stock Manager/stockmanager.db` |
| Settings (DB connection, counters) | `HKCU\Software\EinsteinRobotics\StockManager` | `~/.config/EinsteinRobotics/StockManager.conf` |

The database path comes from `QStandardPaths::AppDataLocation` plus the
organisation/application names set in `main.cpp`; the settings path comes from
the `QSettings("EinsteinRobotics", "StockManager")` used in `src/core/`. The
two use different names for historical reasons — that is expected, not a bug,
and changing either would orphan existing installs' data.

To reset a machine to a clean state, delete both. To move a user to a shared
server instead of the local file, see
[docs/MULTI_COMPUTER_SETUP.md](MULTI_COMPUTER_SETUP.md).

---

## 9. Troubleshooting

### Build

| Symptom | Cause and fix |
|---|---|
| `Could not find a package configuration file provided by "Qt6"` | Qt not on the CMake search path → `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.1/gcc_64` |
| `QXlsx not found at third_party/QXlsx` | incomplete checkout → `git submodule update --init --recursive`, or re-clone |
| `Unknown CMake command "qt_add_qml_module"` | Qt too old or only QtBase installed → install Qt 6.5+ with the Quick/Declarative modules |
| Module `Enstein` not found at runtime, though it builds | a `.qml` file is missing from `QML_FILES` in `qml/CMakeLists.txt` — the module only contains files listed there |
| A `pragma Singleton` QML file fails at runtime | it also needs `QT_QML_SINGLETON_TYPE TRUE` in `set_source_files_properties` at the top of `qml/CMakeLists.txt` |

### Running a development build on Ubuntu

| Symptom | Cause and fix |
|---|---|
| `module "QtQuick" is not installed` | install the `qml6-module-*` packages from section 2 |
| Symbol lookup error, usually under a snap-provided environment | `unset GTK_PATH` before launching |
| Data is not shared with other machines; no error shown | the `QPSQL` driver could not load → install `libqt6sql6-psql` and `libpq5` |
| Nothing appears on a headless/SSH session | `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/EnsteinStockManager` |

### AppImage

| Symptom | Cause and fix |
|---|---|
| `dlopen(): error loading libfuse.so.2` | `sudo apt install libfuse2`, or run with `--appimage-extract-and-run` |
| `Permission denied` | `chmod +x` the AppImage |
| `/lib/x86_64-linux-gnu/libc.so.6: version 'GLIBC_2.3x' not found` | built on a newer distro than the target → rebuild on the oldest Ubuntu you support (CI uses 22.04) |
| `linuxdeployqt` refuses the build with a glibc complaint | keep `-unsupported-allow-new-glibc` (the script already passes it), or use Route B |
| `libqsqlpsql.so` missing from the extracted AppImage | `EXTRA_QT_PLUGINS=sqldrivers` was not exported for Route B |
| `linuxdeploy` errors about the desktop file | `Name=`, `Exec=` and `Icon=` must match the binary and icon filenames in the AppDir |

### Windows package

| Symptom | Cause and fix |
|---|---|
| App exits instantly on an end-user machine, works on the build machine | a missing DLL/plugin/QML module — reproduce with the bare-`PATH` test in 5.4 |
| `vcruntime140.dll was not found` | the MSVC runtime was not bundled; copy it from `$Env:VCToolsRedistDir\x64\Microsoft.VC143.CRT\` |
| Falls back to a local database with no error | `libpq.dll` or `sqldrivers\qsqlpsql.dll` missing from `dist\` (section 5.3) |
| A console window opens behind the UI | `WIN32_EXECUTABLE` was lost from the target properties |
| `ISCC` stops with `StageDir must be defined` | pass all three `/D` definitions (5.5) |
| "Windows protected your PC" on first run | the binaries are unsigned → *More info → Run anyway*. See the code-signing section of [docs/RELEASING.md](RELEASING.md) |
| Installer refuses to run | it is x64-only by design; 32-bit Windows is not supported |

---

## 10. Related documents

| Document | What it covers |
|---|---|
| [docs/RELEASING.md](RELEASING.md) | Cutting a release, artifact choice, code signing |
| [docs/ARCHITECTURE.md](ARCHITECTURE.md) | How the two halves of the project fit together |
| [docs/MULTI_COMPUTER_SETUP.md](MULTI_COMPUTER_SETUP.md) | Pointing a packaged install at a shared server |
| [docs/WINDOWS_SERVER_SETUP_TEST.md](WINDOWS_SERVER_SETUP_TEST.md) | Verifying the server setup on Windows |
| [packaging/linux/README.md](../packaging/linux/README.md) | The AppImage script, in brief |
