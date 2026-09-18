#!/usr/bin/env bash
set -euo pipefail

# Create an AppImage for EnsteinStockManager (Ubuntu/Linux single-file executable)
# Usage: ./create_appimage.sh

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT_DIR=$(cd "$SCRIPT_DIR/../.." && pwd)
BUILD_DIR="$ROOT_DIR/build"
APPDIR="$ROOT_DIR/AppDir"

mkdir -p "$BUILD_DIR"
rm -rf "$APPDIR"

echo "Building Release..."
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --config Release --target EnsteinStockManager -j$(nproc)

echo "Installing into AppDir..."
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

# Copy desktop file and icon
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$ROOT_DIR/packaging/linux/EnsteinStockManager.desktop" "$APPDIR/usr/share/applications/"
cp "$ROOT_DIR/assets/app-icon.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/EnsteinStockManager.png"

echo "AppDir prepared at $APPDIR"

echo "You need linuxdeployqt to bundle Qt and produce the AppImage."
echo "Download linuxdeployqt (.AppImage) from https://github.com/probonopd/linuxdeployqt/releases"
echo "Place it next to this script or in your PATH as 'linuxdeployqt' and make it executable."

if command -v linuxdeployqt >/dev/null 2>&1; then
    LDQ=linuxdeployqt
else
    if [ -x "$SCRIPT_DIR/linuxdeployqt-x86_64.AppImage" ]; then
        LDQ="$SCRIPT_DIR/linuxdeployqt-x86_64.AppImage"
    else
        echo "linuxdeployqt not found. Download it and re-run this script." >&2
        exit 1
    fi
fi

QMAKE_BIN="$(command -v qmake || true)"
if [ -z "$QMAKE_BIN" ] && [ -x "/opt/Qt/6.11.1/gcc_64/bin/qmake" ]; then
    QMAKE_BIN="/opt/Qt/6.11.1/gcc_64/bin/qmake"
fi
if [ -z "$QMAKE_BIN" ]; then
    echo "qmake was not found. Add your Qt bin directory to PATH, then re-run." >&2
    exit 1
fi

echo "Running linuxdeployqt to produce AppImage..."
set -x
"$LDQ" "$APPDIR/usr/bin/EnsteinStockManager" -appimage -qmake="$QMAKE_BIN" -qmldir="$ROOT_DIR/qml" -executable="$APPDIR/usr/bin/EnsteinStockManager" -unsupported-allow-new-glibc
set +x

echo "AppImage build finished. Look for a file named EnsteinStockManager-*.AppImage in the current folder or next to linuxdeployqt."
