# Enstein Stock Manager

**Version:** 2.1.4  
**Company:** Enstein Robots and Automations Pvt Limited

A professional inventory management application built with Qt6 and QML.

**New to the codebase?** Start with
**[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — it explains how the project
is put together and why. Then read the guide for the half you work on:
[backend](docs/BACKEND_GUIDE.md) or [frontend](docs/FRONTEND_GUIDE.md).

## Features

- **Excel File Support**: Create, open, edit, and save `.xlsx` files
- **Stock Management**: Track parts inventory with stock quantities
- **Purchase File Merging**: Record purchases and merge into stock database
- **Search Functionality**: Search parts by name, number, or vendor
- **Cloud Synchronization**: Sync files via shared folders (Dropbox, etc.)
- **User Authentication**: Role-based access control (owner, editor, viewer)
- **Cross-Platform**: Works on Linux, Windows, and macOS

## Project structure

The project is two halves that are built separately and meet only at the
executable. Neither includes the other's code; they meet at runtime through one
object the C++ side publishes to QML as `Backend`.

```
Enstein_Stock_Manager/
├── CMakeLists.txt              ties the two halves into the executable
│
├── src/                        C++ backend  (library: EnsteinCore)
│   ├── app/main.cpp            entry point; registers Backend, loads Main.qml
│   ├── core/                   database, schema, migrations, settings, counters
│   ├── models/                 the stock grid model
│   ├── domain/                 one service per area of the business
│   ├── documents/              purchase order and challan → PDF
│   └── bridge/                 ExcelHandler: the QML-facing contract
│
├── qml/                        QML frontend  (module: EnsteinUi)
│   ├── Main.qml                the window: toolbar, stock grid, footer
│   ├── theme/                  Theme singleton — colours and spacing
│   ├── util/                   Format and App singletons
│   ├── components/             reusable controls
│   └── dialogs/                grouped by area: vendor, item, po, pr, dc, …
│
├── docs/                       architecture and guides — start here
├── assets/                     icons
├── packaging/                  Windows installer and portable build
└── third_party/QXlsx/          vendored .xlsx library
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for what lives where and why,
and [CONTRIBUTING.md](CONTRIBUTING.md) for who owns which directory.

## Prerequisites

### Ubuntu/Debian
```bash
sudo apt install \
    qt6-base-dev \
    qt6-declarative-dev \
    qml6-module-qtquick \
    qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts \
    qml6-module-qtquick-window \
    qml6-module-qtquick-templates \
    qml6-module-qtqml-workerscript \
    cmake \
    build-essential
```

### Windows
- Install Qt 6.5+ from [qt.io](https://www.qt.io/download) (Qt Widgets included)
- Install CMake 3.16+
- Install Visual Studio or MinGW

### macOS
```bash
brew install qt@6 cmake
```

## Build Instructions

QXlsx is vendored under `third_party/`, so there is nothing to clone.

```bash
# Configure  (on Windows/macOS add -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64)
cmake -S . -B build

# Build
cmake --build build -j$(nproc)

# Run
./build/EnsteinStockManager
```

Frontend work can also be linted:

```bash
cmake --build build --target all_qmllint
```

## Usage

### First Run
1. Click **🔒 Login** and enter credentials (default: admin/admin123)
2. Click **📌 Set Permanent** to select your main stock database file
3. Start adding items using **➕ Add Item**

### Creating Files
- **📦 Create Stock**: New inventory file with Stock column
- File includes: Part Name, Part No, Stock, Department, Prepared, Approved, Vendor

### Cloud Sync
1. Click **⚙️** to open Cloud Settings
2. Set your cloud folder path (e.g., Dropbox folder)
3. Enter your user name and role
4. Use **⬆️** to upload and **⬇️** to download

### Searching
1. Click **🔍 Search**
2. Enter part name, number, or vendor
3. Click a result to select that row

## File Columns

| Column | Description |
|--------|-------------|
| Part Name | Name of the component/part |
| Part No | Unique part identifier |
| Stock/Purchase | Quantity in stock or purchased |
| Department | Category (Electronics, Mechanical, etc.) |
| Prepared | Person who prepared the entry |
| Approved | Person who approved the entry |
| Vendor | Supplier name |

## API reference

The frontend talks to the backend through one object, `Backend`, and everything
it may call is listed in
**[docs/QML_API_REFERENCE.md](docs/QML_API_REFERENCE.md)**.

```qml
import ExcelHandler 1.0

var vendors = Backend.getVendorList()
Backend.addItemMasterDetails({ partName: "Bearing", partNo: "BR-01" })
```

## Downloads

Grab a build from the [Releases](../../releases) page.

**Windows** — both `.exe` files are self-contained. The Qt runtime, QML modules,
SQL drivers and PostgreSQL client libraries all travel inside them, so the
target machine needs nothing preinstalled.

- `EnsteinStockManager-Setup-<version>.exe` — installer. Start Menu entry,
  desktop icon, uninstaller, clean upgrades. Use this for normal deployment.
- `EnsteinStockManager-Portable-<version>.exe` — **one file, no install**. Copy
  it to any 64-bit Windows machine and double-click. Needs no admin rights.
- `EnsteinStockManager-Windows-<version>.zip` — the plain deployed folder.

**Linux** — `Enstein_Stock_Manager-<version>-x86_64.AppImage`. `chmod +x` it and run.

On first launch Windows SmartScreen warns about the unsigned binary; choose
*More info -> Run anyway*. See [docs/RELEASING.md](docs/RELEASING.md) for how to
cut a release and how to add code signing.

Your data is not stored next to the exe. The local SQLite fallback database
lives under `%APPDATA%\Enstein Robots and Automations Pvt Limited\Enstein Stock Manager\`,
so it survives upgrades and works from a read-only install location.

## Troubleshooting

### QML modules not found
```bash
sudo apt install qml6-module-qtquick qml6-module-qtquick-controls \
    qml6-module-qtquick-layouts
```

### Symbol lookup error (snap conflict)
```bash
unset GTK_PATH
./EnsteinStockManager
```

### Qt not found during CMake
```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/gcc_64
```

## Documentation

| Document | What it covers |
|---|---|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | How the project is put together, and why — **read first** |
| [docs/BACKEND_GUIDE.md](docs/BACKEND_GUIDE.md) | Working in `src/`: recipes, pitfalls, conventions |
| [docs/FRONTEND_GUIDE.md](docs/FRONTEND_GUIDE.md) | Working in `qml/`: singletons, extracting a dialog |
| [docs/QML_API_REFERENCE.md](docs/QML_API_REFERENCE.md) | Everything QML may call on the backend |
| [docs/DATABASE_SCHEMA.md](docs/DATABASE_SCHEMA.md) | Tables, columns and migrations |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Who owns what, and how the two halves stay in step |
| [docs/MULTI_COMPUTER_SETUP.md](docs/MULTI_COMPUTER_SETUP.md) | Running against a shared server |
| [docs/RELEASING.md](docs/RELEASING.md) | Cutting a release |

## License

Copyright © 2025 Enstein Robots and Automations Pvt Limited.  
All rights reserved.

## Contact

For support, contact: support@einsteinrobotics.com
