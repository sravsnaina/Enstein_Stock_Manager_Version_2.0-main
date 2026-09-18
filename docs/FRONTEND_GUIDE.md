# Frontend guide

Working in `qml/`. Read [ARCHITECTURE.md](ARCHITECTURE.md) first for the shape
of the thing; this is the day-to-day reference.

You own everything under `qml/` and `qml/CMakeLists.txt`. You should never need
to edit anything under `src/` — if you need something the backend does not
offer, ask for it rather than reaching into C++.

---

## Layout

```
qml/
├── CMakeLists.txt          you own this
├── Main.qml                the window: toolbar, stock grid, footer, wiring
├── theme/Theme.qml         colours, spacing, type sizes        (singleton)
├── util/
│   ├── Format.qml          money, quantities, date ranges      (singleton)
│   └── App.qml             the window size                     (singleton)
├── components/             reusable controls
└── dialogs/
    ├── common/             error, login, search, date picker
    ├── vendor/  item/      master data
    ├── po/  pr/  dc/       purchase orders, requests, challans
    ├── inventory/          movements, low stock, issues, reports
    └── settings/           company profile
```

**Every new `.qml` file must be listed in `qml/CMakeLists.txt`.** If it is not,
it is not compiled into the binary and the app fails to find it at runtime with
`Type X unavailable`. This is the single most common mistake.

---

## The four things always in scope

A `.qml` file cannot see ids declared in another `.qml` file. Everything shared
is therefore a singleton:

```qml
import ExcelHandler 1.0     // gives you Backend
import Enstein              // gives you Theme, Format, App
```

| | Use it for | Example |
|---|---|---|
| `Backend` | Anything the application does | `Backend.getVendorList()` |
| `Theme` | Every colour and spacing value | `color: Theme.textSecondary` |
| `Format` | Money, quantities, date ranges | `Format.rupees(row.unitPrice)` |
| `App` | Sizing a dialog against the window | `Math.min(App.windowWidth - 120, 1100)` |

Everything callable on `Backend` is listed in
[QML_API_REFERENCE.md](QML_API_REFERENCE.md).

### Never write a colour literal

```qml
color: "#7f8c8d"            // no
color: Theme.textSecondary  // yes
```

Not for tidiness: the same grey means "secondary text" in sixty places, and when
it changes it has to change in all sixty or the screens drift apart. Names say
what a colour is *for* — `danger` survives the red being adjusted, `red` would
not. If you need a colour `Theme` does not have, add it there.

### Never format money by hand

```qml
text: "₹ " + row.total.toFixed(2)   // no
text: Format.rupees(row.total)      // yes
```

Amounts are grouped the Indian way (`12,34,56,789.00`) to match the printed
documents. `toLocaleString()` will not do this.

---

## Build and check

```bash
cmake --build build -j$(nproc)
./build/EnsteinStockManager
```

QML errors mostly appear at load time, because dialogs are declared objects and
are constructed when `Main.qml` loads. So this catches most mistakes:

```bash
timeout 10 ./build/EnsteinStockManager 2>&1 | grep -iE "error|unavailable|is not defined|TypeError"
```

What it does *not* catch is a mistake inside a function body that nothing calls
at startup. For that, lint:

```bash
cmake --build build --target all_qmllint
```

Ignore the `Unqualified access` noise for now — it is pre-existing and mostly
`model.foo` inside delegates. What matters is anything saying **`was not
found`**, **`is not a type`** or **`unavailable`**: that means a type or
singleton does not resolve, and it will fail at runtime.

---

## Extracting a dialog

This is the recipe used for the twenty-one dialogs already in `dialogs/`, and it
is what the remaining ones need. Work on one dialog at a time and run the app
after each — it takes a minute and saves an hour.

### 1. Find what it depends on

Look for three things inside the dialog's block in `Main.qml`:

- **root properties it reads** (`selectedPODetails`)
- **root properties it writes** (these need care — see step 3)
- **root functions it calls** (`refreshPOList`, `openVendorPicker`)
- **sibling dialogs it opens** (`itemDetailsDialog.showItem(...)`)

### 2. Move the block into a file

Cut the whole `Dialog { … }`, de-indent it one level, and give it the imports:

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein
```

Add `import QtQuick.Controls.Basic 6.3 as BasicControls` only if the body uses
that namespace — the date picker does.

Replace `root.width` / `root.height` with `App.windowWidth` /
`App.windowHeight`; `root` is `Main.qml`'s id and is not visible here.

### 3. Turn each dependency into a declared input or output

**Reads → a property, bound by `Main.qml`:**

```qml
// in the component
property var order

// in Main.qml
PoDetailsDialog { id: poDetailsDialog; order: root.selectedPODetails }
```

**Calls out, including opening a sibling → a signal:**

```qml
// in the component
signal vendorPickerRequested(var field)
...
onClicked: vendorPickerRequested(itemVendorField)

// in Main.qml
ItemMasterDialog {
    id: itemMasterDialog
    onVendorPickerRequested: (field) => openVendorPicker(field)
}
```

**A helper function that only this dialog uses → move it in.** `refreshVendorList`
lives inside `VendorMasterDialog` now, as `refresh()`. Callers elsewhere in
`Main.qml` become `vendorDialog.refresh()`.

**A root property the dialog writes → move it in too.** If the dialog is the
only thing that sets it, it was never really root state. `vendorOriginalName`
became `VendorDetailsDialog.originalName`.

### 4. Register and verify

Add the file to `qml/CMakeLists.txt`, rebuild, run, and check the log is clean.

### A useful trick for refresh methods

A moved `refresh()` is called both by the dialog's own search box (with the
typed text) and by the backend's change signals (which want to keep whatever
filter is showing). Defaulting to the dialog's own field handles both:

```qml
function refresh(filterText) {
    if (filterText === undefined) filterText = itemSearchField.text
    ...
}
```

---

## What is left to extract

`Main.qml` is about 5,300 lines. In rough order of value:

| Still inline | ~Lines | Why it is harder |
|---|---|---|
| `poDialog` | 630 | Owns a cart model, a list model and ~10 root helpers |
| `dcDialog` | 550 | Same shape as `poDialog` |
| `prDialog` | 540 | Same shape as `poDialog` |
| `cloudSettingsDialog` | 190 | Touches many root ids for the connection form |
| `issueDialog` | 160 | Owns cart + search models |
| the pickers (part, vendor, party) | ~350 total | Each owns a `ListModel` and two helpers at root |
| `stockRowEditDialog`, `grnDialog` | ~160 | Moderate |
| the toolbar and the stock grid | ~700 | Screens rather than dialogs; extract into `screens/` |

The three big forms are all the same problem: the dialog, its cart `ListModel`,
its list `ListModel` and its helper functions are one unit that happens to be
spread across root scope. Move all four together into one file and most of the
dependencies disappear — that is what made `ItemMasterDialog` tractable.

---

## Conventions

- One component per file, named after the file, `PascalCase.qml`.
- A component's inputs and outputs go at the very top, above the visual
  properties, so its contract is visible without scrolling.
- Comments explain **why**. `// three pairs per row when there is width for
  them` is useful; `// set columns` is not.
- Prefer `Layout.preferredHeight: grid.implicitHeight + 20` over a fixed number,
  so adding a field later cannot push content out of sight. Both master-data
  forms had already outgrown their hardcoded heights.
- Long text in a list row needs `Layout.fillWidth: true; elide: Text.ElideRight`
  or it runs past the delegate.
