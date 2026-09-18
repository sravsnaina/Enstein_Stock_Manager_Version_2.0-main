# Database schema

Generated from `src/core/dbmanager.cpp` (the DDL) and `src/core/dbschema.cpp`
(the field mapping). If you change one, change the other — see
[BACKEND_GUIDE.md](BACKEND_GUIDE.md#add-a-column-to-an-existing-table).

---

## Two backends, one schema

The same schema runs on either backend:

- **PostgreSQL** — the shared server. Every machine on the LAN connects to one
  of these, which is what makes the data shared and what makes document
  numbering safe: numbers come from the `counters` table, so two people cannot
  mint the same PO number.
- **SQLite** — the per-machine fallback, used when no server is configured or
  reachable. Everything works, but nothing is shared. The UI shows a permanent
  red banner in this state, because silently working alone is the worst
  possible failure here.

The differences are handled in `DatabaseManager`: `BIGSERIAL PRIMARY KEY` vs
`INTEGER PRIMARY KEY AUTOINCREMENT`, and how `tableHasColumn()` interrogates
the catalogue.

> **SQLite does not enforce column types.** It stores whatever was written, so
> a quantity from an old row can come back as the string `"5"`. This is why
> every field carries a type letter and `dbRowsToApp()` coerces on the way
> through. Do not assume a value has the type its column declares.

---

## Tables

### `users`

Owned by **DatabaseManager (authentication)**.

| Column | Type |
|---|---|
| `username` | TEXT PRIMARY KEY |
| `password_hash` | TEXT |
| `role` | TEXT |
| `display_name` | TEXT |

### `counters`

Owned by **Counters (document numbering)**.

| Column | Type |
|---|---|
| `name` | TEXT PRIMARY KEY |
| `value` | INTEGER |

### `vendors`

Owned by **VendorService**.

| Column | Type | Application key |
|---|---|---|
| `vendor_name` | TEXT PRIMARY KEY | `vendorName` |
| `vendor_address` | TEXT | `vendorAddress` |
| `bank_branch` | TEXT | `bankBranch` |
| `ifsc` | TEXT | `ifsc` |
| `account_number` | TEXT | `accountNumber` |
| `cin` | TEXT | `cin` |
| `gstin` | TEXT | `gstin` |
| `pan_number` | TEXT | `panNumber` |
| `pan_name` | TEXT | `panName` |
| `contact_person` | TEXT | `contactPerson` |
| `email` | TEXT | `email` |
| `phone` | TEXT | `phone` |
| `department` | TEXT | `department` |
| `item_category` | TEXT | `itemCategory` |

### `item_master`

Owned by **ItemMasterService**.

| Column | Type | Application key |
|---|---|---|
| `part_no` | TEXT PRIMARY KEY | `partNo` |
| `part_name` | TEXT | `partName` |
| `category` | TEXT | `category` |
| `department` | TEXT | `department` |
| `vendor` | TEXT | `vendor` |
| `required_qty` | INTEGER | `requiredQty` |
| `unit_price` | DOUBLE PRECISION | `unitPrice` |
| `stock_qty` | INTEGER | `stockQty` |
| `hsn_code` † | TEXT | `hsnCode` |
| `unit` † | TEXT | `unit` |
| `item_type` † | TEXT | `itemType` |
| `sac_code` † | TEXT | `sacCode` |

† Added by migration, not by `CREATE TABLE` — see [Migrations](#migrations).

`item_type` is `"Tangible"` or `"Intangible"`. A tangible good is numbered with
an HSN code, an intangible service with a SAC code. Both columns are kept
populated so reclassifying an item and changing your mind does not discard the
number already typed; `ItemMasterService::itemTaxCode()` decides which one a
document prints.

### `purchase_orders`

Owned by **PurchaseOrderService**.

| Column | Type | Application key |
|---|---|---|
| `po_no` | TEXT PRIMARY KEY | `poNo` |
| `po_date` | TEXT | `date` |
| `vendor` | TEXT | `vendor` |
| `part_name` | TEXT | `partName` |
| `part_no` | TEXT | `partNo` |
| `department` | TEXT | `department` |
| `qty` | INTEGER | `qty` |
| `unit_price` | DOUBLE PRECISION | `unitPrice` |
| `total_amount` | DOUBLE PRECISION | `totalAmount` |
| `expected_date` | TEXT | `expectedDate` |
| `expected_end_date` | TEXT | `expectedEndDate` |
| `status` | TEXT | `status` |
| `received_qty` | INTEGER | `receivedQty` |
| `prepared_by` | TEXT | `preparedBy` |
| `approved_by` | TEXT | `approvedBy` |
| `received_by` | TEXT | `receivedBy` |
| `received_date` | TEXT | `receivedDate` |

### `po_items`

Owned by **PurchaseOrderService**.

| Column | Type | Application key |
|---|---|---|
| `po_no` | TEXT | `poNo` |
| `part_name` | TEXT | `partName` |
| `part_no` | TEXT | `partNo` |
| `vendor` | TEXT | `vendor` |
| `department` | TEXT | `department` |
| `qty` | INTEGER | `qty` |
| `unit_price` | DOUBLE PRECISION | `unitPrice` |
| `total_amount` | DOUBLE PRECISION | `totalAmount` |
| `received_qty` | INTEGER | `receivedQty` |

### `grn_records`

Owned by **GoodsReceiptService**.

| Column | Type | Application key |
|---|---|---|
| `grn_no` | TEXT PRIMARY KEY | `grnNo` |
| `po_no` | TEXT | `poNo` |
| `grn_date` | TEXT | `date` |
| `part_name` | TEXT | `partName` |
| `received_qty` | INTEGER | `receivedQty` |
| `accepted_qty` | INTEGER | `acceptedQty` |
| `rejected_qty` | INTEGER | `rejectedQty` |
| `remarks` | TEXT | `remarks` |
| `received_by` | TEXT | `receivedBy` |

### `stock_movements`

Owned by **StockMovementService**.

| Column | Type | Application key |
|---|---|---|
| `mov_date` | TEXT | `date` |
| `part_name` | TEXT | `partName` |
| `part_no` | TEXT | `partNo` |
| `type` | TEXT | `type` |
| `qty` | INTEGER | `qty` |
| `reference` | TEXT | `reference` |
| `done_by` | TEXT | `doneBy` |

### `stock_rows`

Owned by **StockService**.

| Column | Type |
|---|---|
| `part_name` | TEXT |
| `part_no` | TEXT |
| `stock` | INTEGER |
| `department` | TEXT |
| `prepared` | TEXT |
| `approved` | TEXT |
| `vendor` | TEXT |
| `row_date` | TEXT |
| `unit_price` | DOUBLE PRECISION |

### `issue_notes`

Owned by **MaterialIssueService**.

| Column | Type | Application key |
|---|---|---|
| `issue_no` | TEXT | `issueNo` |
| `issue_date` | TEXT | `date` |
| `part_name` | TEXT | `partName` |
| `qty` | INTEGER | `qty` |
| `department` | TEXT | `department` |
| `issued_by` | TEXT | `issuedBy` |

### `delivery_challans`

Owned by **DeliveryChallanService**.

| Column | Type | Application key |
|---|---|---|
| `dc_no` | TEXT PRIMARY KEY | `dcNo` |
| `dc_date` | TEXT | `date` |
| `delivery_time` | TEXT | `deliveryTime` |
| `party_name` | TEXT | `partyName` |
| `party_address` | TEXT | `partyAddress` |
| `party_phone` | TEXT | `partyPhone` |
| `party_email` | TEXT | `partyEmail` |
| `party_gstin` | TEXT | `partyGstin` |
| `ship_name` | TEXT | `shipName` |
| `ship_address` | TEXT | `shipAddress` |
| `ship_phone` | TEXT | `shipPhone` |
| `ship_email` | TEXT | `shipEmail` |
| `ship_gstin` | TEXT | `shipGstin` |
| `terms` | TEXT | `terms` |
| `status` | TEXT | `status` |
| `total_qty` | DOUBLE PRECISION | `totalQty` |
| `prepared_by` | TEXT | `preparedBy` |
| `delivered_by` | TEXT | `deliveredBy` |
| `received_by` | TEXT | `receivedBy` |

### `dc_items`

Owned by **DeliveryChallanService**.

| Column | Type | Application key |
|---|---|---|
| `dc_no` | TEXT | `dcNo` |
| `item_name` | TEXT | `itemName` |
| `part_no` | TEXT | `partNo` |
| `hsn_code` | TEXT | `hsnCode` |
| `qty` | DOUBLE PRECISION | `qty` |
| `unit` | TEXT | `unit` |

### `purchase_requests`

Owned by **PurchaseRequestService**.

| Column | Type | Application key |
|---|---|---|
| `pr_no` | TEXT PRIMARY KEY | `prNo` |
| `pr_date` | TEXT | `date` |
| `requested_by` | TEXT | `requestedBy` |
| `department` | TEXT | `department` |
| `needed_by` | TEXT | `neededBy` |
| `priority` | TEXT | `priority` |
| `status` | TEXT | `status` |
| `remarks` | TEXT | `remarks` |
| `reviewed_by` | TEXT | `reviewedBy` |
| `review_note` | TEXT | `reviewNote` |
| `po_no` | TEXT | `poNo` |

### `pr_items`

Owned by **PurchaseRequestService**.

| Column | Type | Application key |
|---|---|---|
| `pr_no` | TEXT | `prNo` |
| `item_name` | TEXT | `itemName` |
| `part_no` | TEXT | `partNo` |
| `qty` | INTEGER | `qty` |
| `unit` | TEXT | `unit` |
| `estimated_price` | DOUBLE PRECISION | `estimatedPrice` |
| `vendor` | TEXT | `vendor` |

---

## Migrations

`DatabaseManager::createSchema()` runs `CREATE TABLE IF NOT EXISTS` for every
table, then calls `migrateSchema()`. Both run on **every** startup, against both
backends, and both must be safe to run repeatedly.

This is the part that is easy to get wrong: **an existing installation already
has the table, so `CREATE TABLE IF NOT EXISTS` will never add your new column.**
A column added only to the DDL appears on a fresh install and is silently
missing on every machine that already had the app. Every new column needs an
`ALTER TABLE` in `migrateSchema()`.

The guard is `tableHasColumn()`, which is why the same block can run forever
without harm:

```cpp
for (const char *column : {"hsn_code", "unit", "item_type", "sac_code"}) {
    if (tableHasColumn("item_master", column)) continue;
    q.exec(QStringLiteral("ALTER TABLE item_master ADD COLUMN %1 TEXT").arg(...));
}
```

### What has been migrated so far

| Step | Change | Why |
|---|---|---|
| v1 → v2 | `issue_notes` rebuilt with a serial `id` | It used `issue_no` as the primary key, which allowed only one line per issue |
| v1 → v2 | `po_items` backfilled from `purchase_orders` | Orders created before line items existed get one line synthesised from their header |
| v2 → v3 | `purchase_orders.expected_end_date` | The expected date became a delivery period; existing rows keep `expected_date` as the start and have no end until edited |
| v3 → v4 | `item_master.hsn_code`, `.unit` | Delivery challans print both per line, so the item master carries them and the challan autofills |
| v4 → v5 | `item_master.item_type`, `.sac_code` | Goods carry an HSN code, services a SAC code; rows written before this have no type and load as tangible |

### Writing a new one

- Guard with `tableHasColumn()` so it is idempotent.
- Give existing rows a sensible reading rather than a null state the UI cannot
  show. The tangible/intangible split defaults old rows to tangible, because
  that is what every item was before the distinction existed.
- Rebuilding a table (rename → create → copy → drop) is fine, and is what the
  `issue_notes` migration does. Keep the steps in one list so a failure part-way
  is reported with the step that failed.
- Test against a database that predates the change, not only a fresh one. A
  fresh database exercises the DDL; only an old one exercises the migration.
