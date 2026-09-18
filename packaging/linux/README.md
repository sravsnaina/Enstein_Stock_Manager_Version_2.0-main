# Packaging for Linux (AppImage)

This folder contains a script to build a single-file AppImage for Ubuntu/Linux.

Requirements
- A working Qt6 development environment and tools (Qt available in your system).
- `cmake` and a compiler toolchain installed.
- `linuxdeployqt` AppImage (download from https://github.com/probonopd/linuxdeployqt/releases) or available in PATH.

Quick steps

1. Make the script executable:

```bash
chmod +x packaging/linux/create_appimage.sh
```

2. (Optional) Download `linuxdeployqt-x86_64.AppImage` into `packaging/linux/` and make it executable, or ensure `linuxdeployqt` is in your PATH.

3. Run the script from the repository root:

```bash
./packaging/linux/create_appimage.sh
```

The script will build the project in `build/`, create an `AppDir` and then call `linuxdeployqt` to bundle dependencies and produce an `EnsteinStockManager-*.AppImage` file.
