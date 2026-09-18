# Architecture

This is the document to read first. It explains how the application is put
together and, more usefully, *why* — so that when you need to add something you
know where it goes without asking.

---

## The one idea

The project is split into two halves that are built separately and never
include each other's code:

```
src/    EnsteinCore    C++    database, business rules, PDFs
qml/    EnsteinUi      QML    screens, dialogs, controls
```

They meet in exactly one place: a single object the C++ side publishes to QML
under the name **`Backend`**. Everything the UI can ask the application to do is
a method on that object, and nothing else crosses the line.

```
┌──────────────────────────────┐
│  qml/        EnsteinUi       │   Main.qml, dialogs/, components/
│                              │
│      calls Backend.xxx()     │
└──────────────┬───────────────┘
               │  the contract  →  docs/QML_API_REFERENCE.md
┌──────────────▼───────────────┐
│  src/bridge/ ExcelHandler    │   thin: one line per method
├──────────────────────────────┤
│  src/domain/ services        │   the business rules live here
├──────────────────────────────┤
│  src/core/   DatabaseManager │   connections, schema, migrations
└──────────────────────────────┘
```

This is why two people can work on this at once. The backend developer changes
anything under `src/` and only has to keep the bridge's method signatures
stable. The frontend developer changes anything under `qml/` and only has to
call methods that exist. Neither has to read the other's code to make progress,
and the two halves are in different files so they never collide in git.

---

## Why a bridge at all

`ExcelHandler` in `src/bridge/` looks redundant at first — most of its methods
are a single line that forwards to a service:

```cpp
bool ExcelHandler::addVendorDetails(QVariantMap vendor) { return m_vendors->add(vendor); }
```

It earns its place three times over:

1. **It is the written contract.** The frontend developer can read one header
   and know everything they are allowed to call. They never need to open a
   service.
2. **It absorbs renames.** `VendorService::add()` can be renamed, split or
   rewritten without touching a single `.qml` file, because the bridge holds
   the name QML knows.
3. **It translates vocabulary.** Services announce things in their own terms
   (`VendorService::listChanged`). The bridge maps those onto the signal names
   QML already binds to, and fans one out to several where the UI needs it:

   ```cpp
   // Receiving goods moves three things at once.
   connect(m_grn, &GoodsReceiptService::received, this, [this](auto grn, auto po) {
       emit goodsReceived(grn, po);
       emit pendingPOCountChanged();
       emit lowStockCountChanged();
   });
   ```

   A service should not have to know that receiving goods makes a low-stock
   badge stale. The bridge does.

---

## The backend, layer by layer

### `src/core/` — infrastructure

No business rules. Things every domain needs.

| File | What it is |
|---|---|
| `dbmanager.{h,cpp}` | Connections, schema creation, **migrations**, generic table access, document counters |
| `dbschema.{h,cpp}` | Where an application field lives in the database |
| `appsettings.{h,cpp}` | `Counters` (document numbering), `Session` (who is logged in), company profile |
| `serversetup.{h,cpp}` | Turning this machine into the shared PostgreSQL server |

**`dbschema.h` deserves attention.** Two vocabularies describe the same row: QML
and the services speak camelCase keys in a `QVariantMap` (`partNo`,
`unitPrice`), SQL speaks snake_case columns (`part_no`, `unit_price`). One table
matches them up:

```cpp
const QVector<DbField> kItemFields = {
    {"part_no",   "partNo",   's'},
    {"unit_price","unitPrice",'d'},
    ...
};
```

Renaming a column is one edit here instead of a hunt through every query. The
type letter matters: SQLite stores whatever was written regardless of the
declared type, so a quantity from an old row can come back as the string `"5"`
rather than the number `5`. `dbRowsToApp()` coerces on the way through.

### `src/models/` — what the views bind to

`ExcelTableModel` is the stock grid: a plain rows-and-columns table where row 0
holds the column headings, exactly as a spreadsheet does. That is why the QML
grid starts its data at row 1 and why `dataRows` is `rowCount() - 1`.

It is deliberately dumb. It stores cells and notifies views; it knows nothing
about parts or stock levels.

### `src/domain/` — the business

One service per area of the business. Each owns its rows in memory, reads and
writes them through `DatabaseManager`, and enforces the rules that go with them.

`domain/service.h` states the three rules they all follow:

1. **A service owns its rows.** Nothing outside it touches its containers;
   other code asks through public methods. That is what makes a change to how
   vendors are stored a change to one file.
2. **A service knows nothing about QML.** When something goes wrong it emits
   `errorOccurred` with a message meant for a person, and the bridge decides
   whether that becomes a dialog or a log line. That is what lets a service be
   driven from a test or a script.
3. **Dependencies point one way.** A challan needs the item master to fill in an
   HSN code, so `DeliveryChallanService` holds an `ItemMasterService`; the item
   master knows nothing about challans.

Every service is handed the same `AppContext`: the database, the document
numbering, the session, and the stock grid. Passing one struct means adding a
fifth shared thing later is not a change to nine constructors.

### The dependency graph

Arrows point from a service to what it depends on. There are no cycles, and
that is enforced by review, not by the compiler — so check it when you add one.

```
                    ┌───────────────┐   ┌──────────────┐   ┌────────────────────┐
                    │ VendorService │   │ItemMaster    │   │StockMovementService│
                    └───────┬───────┘   │Service       │   └─────────┬──────────┘
                            │           └───┬───┬───┬──┘             │
                            │               │   │   │                │
        ┌───────────────────┴───────────────┘   │   └──────────┐     │
        │                                       │              │     │
┌───────▼──────────────┐   ┌────────────────────▼───┐  ┌───────▼─────▼────────┐
│ PurchaseOrderService │   │ PurchaseRequestService │  │DeliveryChallanService│
└───────┬──────────────┘   └────────────────────────┘  └──────────────────────┘
        │
        │        ┌──────────────┐
        │        │ StockService │◄──────────────┬───────────────┐
        │        └──────┬───────┘               │               │
        │               │                       │               │
┌───────▼───────────────▼──┐  ┌─────────────────┴────┐  ┌───────┴─────────────┐
│  GoodsReceiptService     │  │ MaterialIssueService │  │  LowStockService    │
└──────────────────────────┘  └──────────────────────┘  └─────────────────────┘
```

Two of these are worth explaining:

- **`GoodsReceiptService`** sits downstream of everything because receiving is
  the one operation that touches three domains at once: it writes the received
  quantity back onto the purchase order line, adds accepted stock to the grid,
  and logs a movement for each.
- **`LowStockService`** owns no rows at all. "What are we about to run out of"
  is a question asked across the item master, the stock grid and open orders,
  which is why it holds all three and none of them holds it.

### `src/documents/` — printable output

Builds the HTML for a purchase order or delivery challan and renders it to PDF.
Given plain data, so it can be exercised without a database.

---

## The frontend

### Singletons

A `.qml` file cannot see an object declared in another `.qml` file. That single
fact is what pushes QML applications into one enormous file, and it is why the
things every screen needs are singletons instead:

| Singleton | Import | What it is |
|---|---|---|
| `Backend` | `import ExcelHandler 1.0` | The C++ backend. Registered in `src/app/main.cpp` |
| `Theme` | `import Enstein` | Colours, spacing, type sizes |
| `Format` | `import Enstein` | Money, quantities, date ranges |
| `App` | `import Enstein` | The window size, so a dialog can fit inside it |

`Backend` is registered as a singleton instance rather than a type the UI
creates, for two reasons: there is only ever one of it (it owns the database
connection and every cache, so a second would be a second disagreeing copy of
the application's state), and a singleton is reachable from every file.

### How a dialog declares what it needs

An extracted dialog cannot see `Main.qml`'s ids or its sibling dialogs. Rather
than working around that, each dialog states its dependencies at the top:

```qml
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var order
    property var lineItems

    // Raised for Main.qml to act on: this dialog cannot see its siblings,
    // so anything involving another screen leaves as a signal.
    signal itemChanged()
    signal vendorPickerRequested(var field)
```

and `Main.qml` wires it up:

```qml
ItemMasterDialog {
    id: itemMasterDialog
    onItemChanged: itemMasterDialog.refresh()
    onVendorPickerRequested: (field) => openVendorPicker(field)
}
```

The payoff is that a dialog's inputs and outputs are visible in its first ten
lines, instead of being implicit in whatever happened to be in scope.

---

## A change end to end

Adding the tangible/intangible classification to the item master touched every
layer, and is a good map of where things go:

| Layer | What changed |
|---|---|
| `core/dbmanager.cpp` | `ALTER TABLE item_master ADD COLUMN item_type / sac_code` in `migrateSchema()` |
| `core/dbschema.cpp` | Two `DbField` entries mapping `item_type ↔ itemType`, `sac_code ↔ sacCode` |
| `domain/itemmasterservice.cpp` | `normalizeItemClassification()` and `itemTaxCode()` — the rule about which code a document prints |
| `bridge/excelhandler.cpp` | Nothing. The existing `addItemMasterDetails` already passes the whole map through |
| `qml/dialogs/item/` | Radio pair, and a code field that swaps between HSN and SAC |

Note the bridge row. Because the item master methods take a `QVariantMap`,
adding a field did not change the contract at all — the frontend just started
sending two more keys.

---

## What is deliberately not done yet

Honesty is more useful here than a tidy diagram.

**`Main.qml` is still large (~5,300 lines).** Twenty-one dialogs have been
extracted into `qml/dialogs/`; the three biggest forms — the purchase order,
purchase request and delivery challan entry screens — are still inline, along
with the toolbar and the stock grid. They are the most tangled: each owns a cart
model, a list model and a dozen helper functions at root scope. The recipe for
extracting them is written up in
[FRONTEND_GUIDE.md](FRONTEND_GUIDE.md#extracting-a-dialog).

**`ExcelHandler` still holds the file layer (~1,900 lines).** Spreadsheet
import/export, cloud-folder sync, authentication and LAN server provisioning
have not been moved out. The domain is fully extracted; what remains is
application plumbing that is genuinely bridge-shaped, apart from the `.xlsx`
code, which would make a good `src/io/` library.

**There are no automated tests.** Every change in this refactor was verified by
building and launching the application against the real database and checking it
started clean. That catches crashes and load-time QML errors; it does not catch
a wrong total on a purchase order. The services were extracted specifically so
that this becomes possible — they take an `AppContext` and know nothing about
QML, so a test can construct one against an in-memory SQLite database.

---

## See also

- [QML_API_REFERENCE.md](QML_API_REFERENCE.md) — everything QML may call
- [BACKEND_GUIDE.md](BACKEND_GUIDE.md) — working in `src/`
- [FRONTEND_GUIDE.md](FRONTEND_GUIDE.md) — working in `qml/`
- [DATABASE_SCHEMA.md](DATABASE_SCHEMA.md) — tables and migrations
- [MULTI_COMPUTER_SETUP.md](MULTI_COMPUTER_SETUP.md) — running against a shared server
- [RELEASING.md](RELEASING.md) — cutting a release
