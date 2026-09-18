# Packaging for Linux (AppImage)

`create_appimage.sh` builds a single-file AppImage for Ubuntu/Linux: one
executable carrying the app, the Qt runtime, Qt's QML modules and the SQL
drivers, so the target machine needs nothing preinstalled.

**Full instructions, including the `linuxdeploy` route CI uses and how to
verify a package before handing it out, are in
[docs/BUILDING_EXECUTABLES.md](../../docs/BUILDING_EXECUTABLES.md).** What
follows is the short version.

## What is in this folder

| File | Purpose |
|---|---|
| `create_appimage.sh` | Builds Release, lays out `AppDir/`, calls `linuxdeployqt` |
| `EnsteinStockManager.desktop` | Desktop entry placed in the AppDir; `linuxdeployqt` reads it for the app name and icon |

## Requirements

```bash
sudo apt install -y cmake build-essential \
    qt6-base-dev qt6-declarative-dev \
    qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts qml6-module-qtquick-window \
    qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
    libqt6sql6-sqlite libqt6sql6-psql libpq5 \
    libxcb-cursor0 libfuse2 wget file
```

`libqt6sql6-psql` and `libpq5` must be present on the **build** machine or the
PostgreSQL driver is not bundled, and the packaged app silently falls back to a
local SQLite file instead of the shared server.

The script does not download `linuxdeployqt`; supply it yourself:

```bash
wget -O packaging/linux/linuxdeployqt-x86_64.AppImage \
  https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
chmod +x packaging/linux/linuxdeployqt-x86_64.AppImage
```

It is found either on `PATH` as `linuxdeployqt` or next to this README. `qmake`
must also be on `PATH` so it can locate your Qt installation:

```bash
export PATH=$HOME/Qt/6.10.1/gcc_64/bin:$PATH
```

## Build

Run from the repository root:

```bash
chmod +x packaging/linux/create_appimage.sh
./packaging/linux/create_appimage.sh
```

The output is named from the `Name=` field of the desktop entry:

```
Enstein_Stock_Manager-x86_64.AppImage             # VERSION unset
Enstein_Stock_Manager-2.1.4-x86_64.AppImage       # export VERSION=2.1.4 first
```

## Check it before shipping

```bash
APPIMAGE=$(ls -1 Enstein_Stock_Manager-*.AppImage | head -n1)
chmod +x "$APPIMAGE"
"./$APPIMAGE" --appimage-extract >/dev/null
find squashfs-root -name 'libqsqlpsql.so' -o -name 'libpq.so*' | sort
rm -rf squashfs-root
```

Both must be listed. Then run the AppImage on a machine with no Qt installed —
that is the only test that proves it is self-contained.

An AppImage runs on the distro it was built on and newer, never older. CI
builds on Ubuntu 22.04; build on the oldest Ubuntu you intend to support.
