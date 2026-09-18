# Backend guide

Working in `src/`. Read [ARCHITECTURE.md](ARCHITECTURE.md) first for the shape
of the thing; this is the day-to-day reference.

You own everything under `src/` and `src/CMakeLists.txt`. You should never need
to edit anything under `qml/`.

---

## Layout

```
src/
├── CMakeLists.txt          you own this
├── app/main.cpp            entry point; registers Backend, loads Main.qml
├── core/                   infrastructure, no business rules
│   ├── dbmanager.*         connections, schema, migrations, counters
│   ├── dbschema.*          app field ↔ SQL column mapping
│   ├── appsettings.*       Counters, Session, company profile
│   └── serversetup.*       provisioning this machine as the server
├── models/
│   └── exceltablemodel.*   the stock grid
├── domain/                 one service per area of the business
│   ├── service.h           base class + AppContext + the three rules
│   ├── vendorservice.*
│   ├── itemmasterservice.*
│   ├── purchaseorderservice.*
│   ├── purchaserequestservice.*
│   ├── deliverychallanservice.*
│   ├── goodsreceiptservice.*
│   ├── stockservice.*
│   ├── stockmovementservice.*
│   ├── materialissueservice.*
│   └── lowstockservice.*
├── documents/              PO and challan HTML → PDF
└── bridge/excelhandler.*   the QML-facing surface
```

Headers are included by their path from `src/`, so an include line says which
layer it reaches into:

```cpp
#include "core/dbmanager.h"       // yes
#include "dbmanager.h"            // no
```

---

## Build and check

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/EnsteinStockManager
```

The smoke test that caught the most problems during the refactor is simply
running it and reading the log:

```bash
timeout 10 ./build/EnsteinStockManager 2>&1 | grep -iE "error|segmentation|Loaded"
```

A healthy start prints a `Loaded N …` line for every table. A clean build that
segfaults on launch is usually a null pointer in `AppContext` — see below.

---

## Recipes

### Add a column to an existing table

Three edits, and missing any one of them fails quietly:

1. **`core/dbmanager.cpp`, `migrateSchema()`** — add the `ALTER TABLE`. Existing
   installations already have the table, so `CREATE TABLE IF NOT EXISTS` will
   never add your column:

   ```cpp
   for (const char *column : {"hsn_code", "unit", "item_type", "sac_code"}) {
       if (tableHasColumn("item_master", column)) continue;
       q.exec(QStringLiteral("ALTER TABLE item_master ADD COLUMN %1 TEXT").arg(...));
   }
   ```

2. **`core/dbschema.cpp`** — add the `DbField` entry, or the column is never
   read or written:

   ```cpp
   {"sac_code", "sacCode", 's'},
   ```

3. **The owning service** — if the field has a rule attached (a default, a
   normalisation), put it in the service, not in the migration.

If QML needs the field and the method already takes a `QVariantMap`, there is
nothing to change in the bridge — the map carries it through.

### Add a service

1. Create `domain/xservice.{h,cpp}`, inheriting `Service`.
2. Take `const AppContext &ctx` plus any services you depend on:

   ```cpp
   XService(const AppContext &ctx, ItemMasterService *itemMaster, QObject *parent = nullptr)
       : Service(ctx, parent), m_itemMaster(itemMaster) {}
   ```
3. Add both files to `src/CMakeLists.txt`.
4. In `ExcelHandler`'s constructor, create it **after** its dependencies and
   connect its signals:

   ```cpp
   m_x = new XService(ctx, m_items, this);
   connect(m_x, &Service::errorOccurred, this, &ExcelHandler::errorOccurred);
   connect(m_x, &XService::listChanged,  this, &ExcelHandler::xListChanged);
   ```
5. Add delegating one-liners to the bridge for anything QML needs, and record
   them in [QML_API_REFERENCE.md](QML_API_REFERENCE.md).

Check the dependency graph in [ARCHITECTURE.md](ARCHITECTURE.md#the-dependency-graph)
still has no cycles. If you want an arrow pointing back, emit a signal instead.

### Report a problem to the user

Emit, don't print, and don't show anything yourself:

```cpp
emit errorOccurred("Part number is required");
```

The bridge routes it to the error dialog. Say what failed in the user's words —
they will never see the SQL error, and would not be helped by it if they did.

### Number a new kind of document

Use `Counters`, never a member you increment:

```cpp
int seq = m_counters->next("dc");   // takes the number and advances
int seq = m_counters->peek("dc");   // what the next one would be
```

The database is the authority whenever there is one, which is what stops two
machines on a shared server minting the same number. `peek()` is what a form
should show before the user commits; `next()` is called once, when the document
is actually created.

---

## Things that will bite you

**A null in `AppContext`.** `AppContext` is aggregate-initialised:

```cpp
const AppContext ctx{m_db, m_counters, m_session, m_model};
```

Leave a field off and it is silently `nullptr`, the build stays clean, and the
app segfaults the first time a service uses it. This happened during the
refactor — the stock grid was omitted and `StockService::loadFromDb()` crashed
on the first dereference. Always run the app after touching the constructor.

**`operator[]` on a `QVariantMap` inserts.** Reading a missing key with `[]`
creates a default-constructed entry. Use `.value("key")` for reads; `[]` only
when assigning.

**Service construction order.** Services are created in dependency order in the
constructor. `GoodsReceiptService` needs `PurchaseOrderService` to already
exist, so it comes after it. Getting this wrong gives you a null dependency, not
a compile error.

**Persist after moving stock.** Anything that changes a quantity should call
`StockService::persist()`, not `saveToDb()`. `persist()` writes the grid *and*
announces the change, which is what refreshes the low-stock count and mirrors
the grid to the permanent workbook. `saveToDb()` alone leaves both stale.

**The bridge's method names are a contract.** Renaming a `Q_INVOKABLE` breaks
the frontend with no compile error — QML resolves the name at runtime. If a name
has to change, change it in `qml/` in the same commit and update
[QML_API_REFERENCE.md](QML_API_REFERENCE.md).

---

## Conventions

- Comments explain **why**, not what. The code says what it does.
- One class per file, named after the file.
- `m_` prefix on members; services name their own container after the thing it
  holds (`m_orders`, `m_challans`), not after the table.
- Sort out the header first: a reader should understand a service from its
  header alone, which is why each one opens with a block explaining what the
  area of the business is and what depends on it.
