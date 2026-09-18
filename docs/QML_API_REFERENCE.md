# QML API reference

Everything the frontend may call on the backend. This is the contract between
the two halves of the project: the frontend calls only what is listed here, and
the backend keeps these names and signatures stable.

Everything below is reached through the `Backend` singleton:

```qml
import ExcelHandler 1.0

var vendors = Backend.getVendorList()
Backend.addItemMasterDetails({ partName: "Bearing", partNo: "BR-01", ... })
```

`Backend` is a single instance registered in `src/app/main.cpp`, not a type the
UI creates. See [ARCHITECTURE.md](ARCHITECTURE.md#singletons) for why.

> **Changing anything here is a breaking change.** QML resolves these names at
> runtime, so a rename compiles cleanly and fails when the user clicks the
> button. If a name must change, change every `.qml` call site in the same
> commit and update this file.
>
> This file is generated from `src/bridge/excelhandler.h`. When you add a
> method, add its row here — the header's own comments carry the detail.

---

## Conventions

**Rows are `QVariantMap`s with camelCase keys.** A vendor is
`{ vendorName, vendorAddress, gstin, … }`; an item master row is
`{ partName, partNo, unitPrice, itemType, hsnCode, sacCode, … }`. The mapping
onto SQL columns lives in `src/core/dbschema.cpp`. Because these are maps,
adding a field to a record usually needs no change to this API at all.

**Creating a document returns its number, or an empty string.** `""` means it
was refused and an `errorOccurred` signal has already carried the reason.

**Nothing here shows a dialog.** Failures come back as the `errorOccurred`
signal and `Main.qml` decides what the user sees.

**`get…List()` returns everything; `get…SearchIndex()` returns a lowercase blob
per document** for the search boxes, so filtering as the user types does not
re-scan every line item.

---

## Properties

| Property | Type | Writable | Change signal |
|---|---|---|---|
| `model` | `ExcelTableModel*` | read-only | `-` |
| `currentFile` | `QString` | read-only | `currentFileChanged` |
| `hasUnsavedChanges` | `bool` | read-only | `unsavedChangesChanged` |
| `permanentFile` | `QString` | read-only | `permanentFileChanged` |
| `cloudFolder` | `QString` | yes | `cloudFolderChanged` |
| `syncEnabled` | `bool` | yes | `syncEnabledChanged` |
| `lastSyncTime` | `QString` | read-only | `lastSyncTimeChanged` |
| `syncStatus` | `QString` | read-only | `syncStatusChanged` |
| `currentUser` | `QString` | yes | `currentUserChanged` |
| `userRole` | `QString` | yes | `userRoleChanged` |
| `lowStockCount` | `int` | read-only | `lowStockCountChanged` |
| `pendingPOCount` | `int` | read-only | `pendingPOCountChanged` |
| `pendingRequestCount` | `int` | read-only | `purchaseRequestListChanged` |
| `totalVendors` | `int` | read-only | `vendorListChanged` |

---

## Methods

### Files, import and export

| Method | Returns |
|---|---|
| `createNew(…)` | `void` |
| `createStockFile(…)` | `void` |
| `createPurchaseFile(…)` | `void` |
| `getFileName(…)` | `QString` |
| `getFileType(…)` | `QString` |
| `loadExcel(…)` | `bool` |
| `saveExcel(…)` | `bool` |
| `setPermanentFile(…)` | `bool` |
| `loadPermanentFile(…)` | `bool` |
| `saveToPermanent(…)` | `bool` |
| `getSavedPermanentPath(…)` | `QString` |
| `hasSavedPermanentFile(…)` | `bool` |
| `importStockFile(…)` | `bool` |
| `appendFromFile(…)` | `bool` |
| `appendStockFile(…)` | `bool` |
| `validateFileStructure(…)` | `bool` |
| `browseOpenFile(…)` | `QString` |
| `browseSaveFile(…)` | `QString` |
| `browseFolder(…)` | `QString` |
| `exportReport(…)` | `bool` |
| `searchPartName(…)` | `int` |
| `searchAllMatches(…)` | `QVariantList` |
| `uploadFileForPart(…)` | `bool` |
| `getUploadedFilePath(…)` | `QString` |
| `openUploadedFile(…)` | `bool` |
| `hasUploadedFile(…)` | `bool` |
| `addNewItem(…)` | `void` |
| `getNextSerialNumber(…)` | `int` |

### Authentication (backed by the shared users table)

| Method | Returns |
|---|---|
| `login(…)` | `QString` |

### Database connection settings

| Method | Returns |
|---|---|
| `getDatabaseSettings(…)` | `QVariantMap` |
| `configureDatabase(…)` | `bool` |
| `databaseStatus(…)` | `QString` |
| `isDatabaseConnected(…)` | `bool` |
| `isDatabaseServerBackend(…)` | `bool` |
| `copyLocalDataToServer(…)` | `QVariantMap` |
| `databaseLastError(…)` | `QString` |

### Stock segregation for the main window

| Method | Returns |
|---|---|
| `departmentStockSummary(…)` | `QVariantList` |
| `stockRowsForDepartment(…)` | `QVariantList` |
| `stockTotals(…)` | `QVariantMap` |
| `refreshFromDatabase(…)` | `void` |
| `setLiveSyncInterval(…)` | `void` |
| `liveSyncInterval(…)` | `int` |

### LAN server provisioning

| Method | Returns |
|---|---|
| `setupThisComputerAsServer(…)` | `void` |
| `isServerProvisioned(…)` | `bool` |
| `serverLanAddressHint(…)` | `QString` |

### Cloud Sync (deprecated: storage now lives in the shared database)

| Method | Returns |
|---|---|
| `syncToCloud(…)` | `bool` |
| `syncFromCloud(…)` | `bool` |
| `checkForUpdates(…)` | `bool` |
| `getCloudFilePath(…)` | `QString` |
| `canEdit(…)` | `bool` |
| `setCloudFolder(…)` | `void` |
| `setCurrentUser(…)` | `void` |
| `setUserRole(…)` | `void` |
| `setSyncEnabled(…)` | `void` |

### Vendor Management

| Method | Returns |
|---|---|
| `addVendorDetails(…)` | `bool` |
| `updateVendorDetails(…)` | `bool` |
| `deleteVendor(…)` | `bool` |
| `getVendorList(…)` | `QVariantList` |
| `getVendorNames(…)` | `QStringList` |
| `getVendorByName(…)` | `QVariantMap` |

### Item Master Management

| Method | Returns |
|---|---|
| `addItemMasterDetails(…)` | `bool` |
| `updateItemMasterDetails(…)` | `bool` |
| `getItemMasterList(…)` | `QVariantList` |
| `deleteItem(…)` | `bool` |

### Purchase Order Management

| Method | Returns |
|---|---|
| `createPurchaseOrderItems(…)` | `QString` |
| `getPOItems(…)` | `QVariantList` |
| `createPurchaseOrder(…)` | `QString` |
| `sendPOForApproval(…)` | `bool` |
| `getPOList(…)` | `QVariantList` |
| `getPOSearchIndex(…)` | `QVariantMap` |
| `updatePOStatus(…)` | `bool` |
| `updatePurchaseOrder(…)` | `bool` |
| `getPOByNumber(…)` | `QVariantMap` |
| `getNextPONumber(…)` | `QString` |

### Printable purchase order

| Method | Returns |
|---|---|
| `getCompanyProfile(…)` | `QVariantMap` |
| `saveCompanyProfile(…)` | `bool` |
| `generatePOPreview(…)` | `QVariantMap` |
| `savePOPdf(…)` | `QString` |
| `defaultPOPdfPath(…)` | `QString` |
| `openInSystemViewer(…)` | `bool` |

### Delivery Challan (DC)

| Method | Returns |
|---|---|
| `createDeliveryChallan(…)` | `QString` |
| `updateDeliveryChallan(…)` | `bool` |
| `deleteDeliveryChallan(…)` | `bool` |
| `updateDCStatus(…)` | `bool` |
| `getDCList(…)` | `QVariantList` |
| `getDCSearchIndex(…)` | `QVariantMap` |
| `getDCItems(…)` | `QVariantList` |
| `getDCByNumber(…)` | `QVariantMap` |
| `getNextDCNumber(…)` | `QString` |
| `getDCPartyList(…)` | `QVariantList` |
| `generateDCPreview(…)` | `QVariantMap` |
| `saveDCPdf(…)` | `QString` |
| `defaultDCPdfPath(…)` | `QString` |

### Purchase Request (PR)

| Method | Returns |
|---|---|
| `createPurchaseRequest(…)` | `QString` |
| `updatePurchaseRequest(…)` | `bool` |
| `deletePurchaseRequest(…)` | `bool` |
| `setPurchaseRequestStatus(…)` | `bool` |
| `linkRequestToPO(…)` | `bool` |
| `getPRList(…)` | `QVariantList` |
| `getPRSearchIndex(…)` | `QVariantMap` |
| `getPRItems(…)` | `QVariantList` |
| `getPRByNumber(…)` | `QVariantMap` |
| `getNextPRNumber(…)` | `QString` |

### Goods Receipt Note (GRN)

| Method | Returns |
|---|---|
| `receiveGoodsForItem(…)` | `QString` |
| `receiveGoods(…)` | `QString` |
| `getGRNList(…)` | `QVariantList` |

### Stock Movement Log

| Method | Returns |
|---|---|
| `logStockMovement(…)` | `bool` |
| `getStockMovements(…)` | `QVariantList` |
| `getAllMovements(…)` | `QVariantList` |

### Material Issue

| Method | Returns |
|---|---|
| `issueMultipleStock(…)` | `QString` |
| `issueStock(…)` | `QString` |
| `getIssueNotes(…)` | `QVariantList` |

### Low Stock Alerts

| Method | Returns |
|---|---|
| `getLowStockItems(…)` | `QVariantList` |
| `lowStockCount(…)` | `int` |
| `autoGeneratePOForLowStock(…)` | `bool` |

---

## Signals

Connect to these with a `Connections { target: Backend }` block. `Main.qml`
already handles most of them centrally.

| Signal | Parameters |
|---|---|
| `currentFileChanged` | `` |
| `unsavedChangesChanged` | `` |
| `permanentFileChanged` | `` |
| `errorOccurred` | `const QString &error` |
| `fileLoaded` | `const QString &fileName` |
| `fileSaved` | `const QString &fileName` |
| `fileMerged` | `const QString &fileName, int rowsAdded, int rowsUpdated` |
| `searchResultFound` | `int row` |
| `sharedDataChanged` | `` |
| `cloudFolderChanged` | `` |
| `syncEnabledChanged` | `` |
| `lastSyncTimeChanged` | `` |
| `syncStatusChanged` | `` |
| `currentUserChanged` | `` |
| `userRoleChanged` | `` |
| `syncCompleted` | `bool success` |
| `conflictDetected` | `const QString &message` |
| `serverProvisionProgress` | `const QString &message` |
| `serverProvisionFinished` | `bool success, const QVariantMap &result` |
| `lowStockCountChanged` | `` |
| `pendingPOCountChanged` | `` |
| `vendorListChanged` | `` |
| `itemMasterListChanged` | `` |
| `purchaseOrderCreated` | `const QString &poNo` |
| `deliveryChallanCreated` | `const QString &dcNo` |
| `deliveryChallanListChanged` | `` |
| `purchaseRequestCreated` | `const QString &prNo` |
| `purchaseRequestListChanged` | `` |
| `goodsReceived` | `const QString &grnNo, const QString &poNo` |
| `stockIssued` | `const QString &issueNo, const QString &partName, int qty` |
| `movementLogged` | `const QString &partName, const QString &type, int qty` |

---

## Notes on particular areas

**Item master and tax codes.** An item is either a tangible good, numbered with
an HSN code, or an intangible service, numbered with a SAC code. Rows carry
`itemType` (`"Tangible"` / `"Intangible"`), `hsnCode` and `sacCode`. Both codes
are kept when the classification changes, so reclassifying and changing your
mind does not discard a number. `getItemMasterList()` adds a resolved `taxCode`
key so a screen never has to decide HSN-or-SAC itself.

**Documents and line items.** Purchase orders, delivery challans and purchase
requests are each a header plus lines. Create and update take the header map
and the list of lines together; the header's totals are always recomputed from
the lines, never stored independently.

**Cloud sync is deprecated.** `syncToCloud`, `syncFromCloud`, `cloudFolder` and
friends predate the shared database and remain only so existing installations
keep working. New work should use the shared PostgreSQL server — see
[MULTI_COMPUTER_SETUP.md](MULTI_COMPUTER_SETUP.md).
