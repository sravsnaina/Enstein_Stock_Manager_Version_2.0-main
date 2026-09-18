#ifndef EXCELHANDLER_H
#define EXCELHANDLER_H

#include <QObject>
#include <QAbstractTableModel>
#include <QVariant>
#include <QVector>
#include <QDebug>
#include <QUrl>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QDesktopServices>
#include <QDateTime>
#include <QSet>
#include <QTimer>

#include "models/exceltablemodel.h"

#include "domain/service.h"

class Counters;
class Session;
class DatabaseManager;
class ServerSetup;
class VendorService;
class ItemMasterService;
class DeliveryChallanService;
class PurchaseRequestService;
class StockMovementService;
class StockService;
class MaterialIssueService;
class PurchaseOrderService;
class GoodsReceiptService;
class LowStockService;

// ==================== ExcelHandler ====================
class ExcelHandler : public QObject
{
    Q_OBJECT

    // Existing Properties
    Q_PROPERTY(ExcelTableModel* model READ model CONSTANT)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY unsavedChangesChanged)
    Q_PROPERTY(QString permanentFile READ permanentFile NOTIFY permanentFileChanged)

    // Cloud Sync Properties
    Q_PROPERTY(QString cloudFolder READ cloudFolder WRITE setCloudFolder NOTIFY cloudFolderChanged)
    Q_PROPERTY(bool syncEnabled READ syncEnabled WRITE setSyncEnabled NOTIFY syncEnabledChanged)
    Q_PROPERTY(QString lastSyncTime READ lastSyncTime NOTIFY lastSyncTimeChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncStatusChanged)
    Q_PROPERTY(QString currentUser READ currentUser WRITE setCurrentUser NOTIFY currentUserChanged)
    Q_PROPERTY(QString userRole READ userRole WRITE setUserRole NOTIFY userRoleChanged)

    // Supply Chain Properties
    Q_PROPERTY(int lowStockCount READ lowStockCount NOTIFY lowStockCountChanged)
    Q_PROPERTY(int pendingPOCount READ pendingPOCount NOTIFY pendingPOCountChanged)
    Q_PROPERTY(int pendingRequestCount READ pendingRequestCount NOTIFY purchaseRequestListChanged)
    Q_PROPERTY(int totalVendors READ totalVendors NOTIFY vendorListChanged)

public:
    explicit ExcelHandler(QObject *parent = nullptr);
    ~ExcelHandler() override;

    ExcelTableModel* model() const { return m_model; }
    QString currentFile() const { return m_currentFile; }
    bool hasUnsavedChanges() const { return m_hasUnsavedChanges; }
    QString permanentFile() const { return m_permanentFile; }

    // ---- Existing Methods ----
    Q_INVOKABLE void createNew(int rows = 10, int cols = 10);
    Q_INVOKABLE void createStockFile(int rows = 15);
    Q_INVOKABLE void createPurchaseFile(int rows = 15);
    Q_INVOKABLE QString getFileName() const;
    Q_INVOKABLE QString getFileType() const;
    Q_INVOKABLE bool loadExcel(const QString &filePath);
    Q_INVOKABLE bool saveExcel(const QString &filePath = QString());

    Q_INVOKABLE bool setPermanentFile(const QString &filePath);
    Q_INVOKABLE bool loadPermanentFile();
    Q_INVOKABLE bool saveToPermanent();
    Q_INVOKABLE QString getSavedPermanentPath();
    Q_INVOKABLE bool hasSavedPermanentFile();

    // Imports a stock xlsx into the shared database (replaces the old
    // "permanent file" concept: the database is the permanent store now).
    Q_INVOKABLE bool importStockFile(const QString &filePath);

    Q_INVOKABLE bool appendFromFile(const QString &filePath);
    Q_INVOKABLE bool appendStockFile(const QString &filePath);
    Q_INVOKABLE bool validateFileStructure(const QString &filePath);
    Q_INVOKABLE QString browseOpenFile(const QString &title, const QString &filter);
    Q_INVOKABLE QString browseSaveFile(const QString &title, const QString &filter);
    Q_INVOKABLE QString browseFolder(const QString &title);
    Q_INVOKABLE bool exportReport(const QString &fromDate, const QString &toDate, const QString &filePath);

    Q_INVOKABLE int searchPartName(const QString &partName);
    Q_INVOKABLE QVariantList searchAllMatches(const QString &searchText);

    Q_INVOKABLE bool uploadFileForPart(int row, const QString &filePath);
    Q_INVOKABLE QString getUploadedFilePath(int row);
    Q_INVOKABLE bool openUploadedFile(int row);
    Q_INVOKABLE bool hasUploadedFile(int row);

    Q_INVOKABLE void addNewItem(const QString &partName,
                                const QString &category,
                                int quantity,
                                double unitPrice);
    Q_INVOKABLE int getNextSerialNumber() const;

    // ---- Authentication (backed by the shared users table) ----
    Q_INVOKABLE QString login(const QString &username, const QString &password);

    // ---- Database connection settings ----
    Q_INVOKABLE QVariantMap getDatabaseSettings() const;
    Q_INVOKABLE bool configureDatabase(const QString &driver,
                                       const QString &host, int port,
                                       const QString &name,
                                       const QString &user, const QString &password);
    Q_INVOKABLE QString databaseStatus() const;
    Q_INVOKABLE bool isDatabaseConnected() const;
    // True only when actually connected to a shared server (PostgreSQL), not
    // the local SQLite fallback. Lets the UI detect a silent fallback.
    Q_INVOKABLE bool isDatabaseServerBackend() const;
    // Copies this computer's local data into the shared server it is connected
    // to, for the one-time switch from single-machine storage to a server.
    Q_INVOKABLE QVariantMap copyLocalDataToServer();
    // Human-readable reason the last connection attempt failed (empty if none).
    Q_INVOKABLE QString databaseLastError() const;
    // Reloads all supply-chain caches from the database (pull latest in multi-user).
    // ---- Stock segregation for the main window ----
    // Per-department rollup: {department, parts, qty, value, share, colorIndex}
    // sorted by quantity. colorIndex is derived from the department name, not
    // from its rank, so a department keeps its colour as the mix changes.
    Q_INVOKABLE QVariantList departmentStockSummary() const;
    // 1-based stock row numbers for one department; empty department = all rows.
    Q_INVOKABLE QVariantList stockRowsForDepartment(const QString &department) const;
    // Headline totals across all stock: {parts, units, value, departments}
    Q_INVOKABLE QVariantMap stockTotals() const;

    Q_INVOKABLE void refreshFromDatabase();
    // Seconds between checks for other machines' edits. 0 stops the polling.
    Q_INVOKABLE void setLiveSyncInterval(int seconds);
    Q_INVOKABLE int liveSyncInterval() const;

    // ---- LAN server provisioning ----
    // Turns this computer into the shared PostgreSQL server for every other
    // installation of the app. Runs asynchronously; connect to
    // serverProvisionProgress()/serverProvisionFinished() for status.
    Q_INVOKABLE void setupThisComputerAsServer();
    Q_INVOKABLE bool isServerProvisioned() const;
    Q_INVOKABLE QString serverLanAddressHint() const;

    // ---- Cloud Sync (deprecated: storage now lives in the shared database) ----
    Q_INVOKABLE bool syncToCloud();
    Q_INVOKABLE bool syncFromCloud();
    Q_INVOKABLE bool checkForUpdates();
    Q_INVOKABLE QString getCloudFilePath() const;
    Q_INVOKABLE bool canEdit() const;
    Q_INVOKABLE void setCloudFolder(const QString &folder);
    Q_INVOKABLE void setCurrentUser(const QString &username);
    Q_INVOKABLE void setUserRole(const QString &role);
    Q_INVOKABLE void setSyncEnabled(bool enabled);

    QString cloudFolder() const { return m_cloudFolder; }
    bool syncEnabled() const { return m_syncEnabled; }
    QString lastSyncTime() const { return m_lastSyncTime; }
    QString syncStatus() const { return m_syncStatus; }
    QString currentUser() const;
    QString userRole() const;

    // ==================== SUPPLY CHAIN METHODS ====================

    // ---- Vendor Management ----
    Q_INVOKABLE bool addVendorDetails(QVariantMap vendor);
    Q_INVOKABLE bool updateVendorDetails(QVariantMap vendor);

    Q_INVOKABLE bool deleteVendor(const QString &name);
    Q_INVOKABLE QVariantList getVendorList();
    Q_INVOKABLE QStringList getVendorNames();
    Q_INVOKABLE QVariantMap getVendorByName(const QString &name);
    int totalVendors() const;
    
    // ---- Item Master Management ----
    Q_INVOKABLE bool addItemMasterDetails(QVariantMap itemDetails);
    Q_INVOKABLE bool updateItemMasterDetails(QVariantMap itemDetails);
    Q_INVOKABLE QVariantList getItemMasterList();
    Q_INVOKABLE bool deleteItem(const QString &partName);
    //Q_INVOKABLE bool updateItemMasterDetails(const QString &partNo, QVariantMap itemDetails);

    // ---- Purchase Order Management ----
    // One PO holding several line items; each item carries its own vendor.
    // items: list of maps {partName, partNo, vendor, department, qty, unitPrice}
    // expectedDate/expectedEndDate bound the period the goods are needed in.
    // An empty end date means a single expected day rather than a range.
    Q_INVOKABLE QString createPurchaseOrderItems(const QVariantList &items,
                                                 const QString &expectedDate,
                                                 const QString &expectedEndDate,
                                                 const QString &preparedBy = "");
    Q_INVOKABLE QVariantList getPOItems(const QString &poNo);

    // Legacy single-item entry point (delegates to createPurchaseOrderItems).
    Q_INVOKABLE QString createPurchaseOrder(const QString &vendor,
                                            const QString &partName,
                                            const QString &partNo,
                                            int qty, double unitPrice,
                                            const QString &expectedDate,
                                            const QString &department = "",
                                            const QString &preparedBy = "",
                                            const QString &expectedEndDate = "");
    Q_INVOKABLE bool sendPOForApproval(const QString &poNo, const QString &approvedBy);
    Q_INVOKABLE QVariantList getPOList(const QString &statusFilter = "");
    // poNo -> one lowercase blob of everything the PO list can be searched by,
    // including the line items the aggregated "+N more" row label hides. Built
    // in a single pass so the search box stays cheap as the history grows.
    Q_INVOKABLE QVariantMap getPOSearchIndex() const;
    Q_INVOKABLE bool updatePOStatus(const QString &poNo, const QString &newStatus);
    Q_INVOKABLE bool updatePurchaseOrder(const QString &poNo, const QVariantMap &poDetails);
    Q_INVOKABLE QVariantMap getPOByNumber(const QString &poNo);
    Q_INVOKABLE QString getNextPONumber();

    // ---- Printable purchase order ----
    // Company details that head every printed document. Stored in QSettings so
    // they survive reinstalls and are shared by every document type.
    Q_INVOKABLE QVariantMap getCompanyProfile() const;
    Q_INVOKABLE bool saveCompanyProfile(const QVariantMap &profile);

    // Renders the PO to a PDF in a temp folder and rasterises its pages for the
    // preview dialog. Returns { poNo, pdfPath, pages: [image urls] }, or an
    // empty map if the PO could not be found or rendered.
    Q_INVOKABLE QVariantMap generatePOPreview(const QString &poNo,
                                              const QString &comments = QString());
    // Writes the PO PDF to its permanent home. An empty destPath uses
    // defaultPOPdfPath(). Returns the saved path, or "" on failure.
    Q_INVOKABLE QString savePOPdf(const QString &poNo,
                                  const QString &destPath = QString(),
                                  const QString &comments = QString());
    Q_INVOKABLE QString defaultPOPdfPath(const QString &poNo) const;
    Q_INVOKABLE bool openInSystemViewer(const QString &path);
    int pendingPOCount() const;

    // ---- Delivery Challan (DC) ----
    // The note that travels with goods leaving the premises. It is a document
    // only: stock is not moved by raising one, because what leaves the shelf is
    // already accounted for by Issue Stock and the GRN trail.
    //
    // challan: partyName, partyAddress, partyPhone, partyEmail, partyGstin,
    //          shipName, shipAddress, shipPhone, shipEmail, shipGstin,
    //          date, deliveryTime, terms, preparedBy, deliveredBy, receivedBy
    // items:   list of maps {itemName, partNo, hsnCode, qty, unit}
    Q_INVOKABLE QString createDeliveryChallan(const QVariantMap &challan,
                                              const QVariantList &items);
    // Rewrites a challan and its lines. Only a Draft may be changed, so a
    // challan that has already travelled with the goods can never be rewritten.
    Q_INVOKABLE bool updateDeliveryChallan(const QString &dcNo,
                                           const QVariantMap &challan,
                                           const QVariantList &items);
    Q_INVOKABLE bool deleteDeliveryChallan(const QString &dcNo);
    Q_INVOKABLE bool updateDCStatus(const QString &dcNo, const QString &newStatus);
    Q_INVOKABLE QVariantList getDCList(const QString &statusFilter = "");
    // dcNo -> one lowercase blob of everything the challan list can be searched
    // by, including the line items the "+N more" row label hides.
    Q_INVOKABLE QVariantMap getDCSearchIndex() const;
    Q_INVOKABLE QVariantList getDCItems(const QString &dcNo);
    Q_INVOKABLE QVariantMap getDCByNumber(const QString &dcNo);
    Q_INVOKABLE QString getNextDCNumber();
    // Parties delivered to before, most recent first, so a repeat delivery does
    // not mean retyping an address.
    Q_INVOKABLE QVariantList getDCPartyList() const;

    // Renders the challan to a PDF in a temp folder and rasterises its pages
    // for the preview dialog. Returns { dcNo, pdfPath, pages: [image urls] },
    // or an empty map if the challan could not be found or rendered.
    Q_INVOKABLE QVariantMap generateDCPreview(const QString &dcNo);
    // Writes the challan PDF to its permanent home. An empty destPath uses
    // defaultDCPdfPath(). Returns the saved path, or "" on failure.
    Q_INVOKABLE QString saveDCPdf(const QString &dcNo,
                                  const QString &destPath = QString());
    Q_INVOKABLE QString defaultDCPdfPath(const QString &dcNo) const;

    // ---- Purchase Request (PR) ----
    // What anyone on the floor needs bought. Requests are visible to everyone;
    // the supply chain team reviews them and turns an approved one into a
    // purchase order, which links the two together for good.
    //
    // request: department, neededBy, priority, remarks, requestedBy
    // items:   list of maps {itemName, partNo, qty, unit, estimatedPrice, vendor}
    Q_INVOKABLE QString createPurchaseRequest(const QVariantMap &request,
                                              const QVariantList &items);
    // Rewrites a request and its lines. Only a Pending request may be changed,
    // so nothing shifts under a reviewer after they have acted on it.
    Q_INVOKABLE bool updatePurchaseRequest(const QString &prNo,
                                           const QVariantMap &request,
                                           const QVariantList &items);
    Q_INVOKABLE bool deletePurchaseRequest(const QString &prNo);
    // Approve or reject a request, recording who decided and why.
    Q_INVOKABLE bool setPurchaseRequestStatus(const QString &prNo,
                                              const QString &newStatus,
                                              const QString &reviewedBy = QString(),
                                              const QString &note = QString());
    // Records that prNo was ordered as poNo. Called once the order exists, so a
    // failed order never leaves a request marked as bought.
    Q_INVOKABLE bool linkRequestToPO(const QString &prNo, const QString &poNo);
    Q_INVOKABLE QVariantList getPRList(const QString &statusFilter = "");
    // prNo -> one lowercase blob of everything the request list can be searched
    // by, including the line items the "+N more" row label hides.
    Q_INVOKABLE QVariantMap getPRSearchIndex() const;
    Q_INVOKABLE QVariantList getPRItems(const QString &prNo);
    Q_INVOKABLE QVariantMap getPRByNumber(const QString &prNo);
    Q_INVOKABLE QString getNextPRNumber();
    int pendingRequestCount() const;

    // ---- Goods Receipt Note (GRN) ----
    // Receive against a specific PO line item (multi-item POs).
    Q_INVOKABLE QString receiveGoodsForItem(int itemId,
                                            int receivedQty, int acceptedQty,
                                            int rejectedQty, const QString &remarks,
                                            const QString &receivedBy = "");
    // Legacy whole-PO entry point (delegates to the first open line).
    Q_INVOKABLE QString receiveGoods(const QString &poNo,
                                     int receivedQty, int acceptedQty,
                                     int rejectedQty, const QString &remarks,
                                     const QString &receivedBy = "");
    Q_INVOKABLE QVariantList getGRNList();

    // ---- Stock Movement Log ----
    Q_INVOKABLE bool logStockMovement(const QString &partName, const QString &partNo,
                                      const QString &movementType, int qty,
                                      const QString &reference, const QString &doneBy);
    Q_INVOKABLE QVariantList getStockMovements(const QString &partNameFilter = "");
    Q_INVOKABLE QVariantList getAllMovements();

    // ---- Material Issue ----
    // Issue several parts to one department under a single issue number.
    // items: list of maps {partName, qty}
    Q_INVOKABLE QString issueMultipleStock(const QVariantList &items,
                                           const QString &department,
                                           const QString &issuedBy);
    Q_INVOKABLE QString issueStock(const QString &partName, int qty,
                                   const QString &department, const QString &issuedBy);
    Q_INVOKABLE QVariantList getIssueNotes();

    // ---- Low Stock Alerts ----
    Q_INVOKABLE QVariantList getLowStockItems();
    Q_INVOKABLE int lowStockCount();
    Q_INVOKABLE bool autoGeneratePOForLowStock();

signals:
    void currentFileChanged();
    void unsavedChangesChanged();
    void permanentFileChanged();
    void errorOccurred(const QString &error);
    void fileLoaded(const QString &fileName);
    void fileSaved(const QString &fileName);
    void fileMerged(const QString &fileName, int rowsAdded, int rowsUpdated);
    void searchResultFound(int row);
    // Another machine changed the shared data and it has just been reloaded.
    void sharedDataChanged();

    // Cloud signals
    void cloudFolderChanged();
    void syncEnabledChanged();
    void lastSyncTimeChanged();
    void syncStatusChanged();
    void currentUserChanged();
    void userRoleChanged();
    void syncCompleted(bool success);
    void conflictDetected(const QString &message);

    // LAN server provisioning
    void serverProvisionProgress(const QString &message);
    void serverProvisionFinished(bool success, const QVariantMap &result);

    // Supply Chain signals
    void lowStockCountChanged();
    void pendingPOCountChanged();
    void vendorListChanged();
    void itemMasterListChanged();
    void purchaseOrderCreated(const QString &poNo);
    void deliveryChallanCreated(const QString &dcNo);
    void deliveryChallanListChanged();
    void purchaseRequestCreated(const QString &prNo);
    void purchaseRequestListChanged();
    void goodsReceived(const QString &grnNo, const QString &poNo);
    void stockIssued(const QString &issueNo, const QString &partName, int qty);
    void movementLogged(const QString &partName, const QString &type, int qty);

private slots:
    void onModelDataChanged();                           
    void autoSavePermanent();
    void autoSyncFromCloud();

private:
    DatabaseManager *m_db;
    ServerSetup *m_serverSetup;
    // Owned. Held by pointer so the header need not see their
    // definitions; released in the destructor.
    Counters *m_counters;
    Session *m_session;

    // ---- Domain services ------------------------------------------------
    // Each owns one area of the business and its rows; the methods above are
    // thin delegations onto them. See docs/ARCHITECTURE.md.
    VendorService *m_vendors;
    ItemMasterService *m_items;
    DeliveryChallanService *m_challans;
    PurchaseRequestService *m_requests;
    StockMovementService *m_movements;
    StockService *m_stock;
    MaterialIssueService *m_issues;
    PurchaseOrderService *m_orders;
    GoodsReceiptService *m_grn;
    LowStockService *m_lowStock;
    ExcelTableModel *m_model;
    QString m_currentFile;
    QString m_permanentFile;
    bool m_hasUnsavedChanges;
    QString m_uploadsDir;

    // Cloud sync
    QString m_cloudFolder;
    bool m_syncEnabled;
    QString m_lastSyncTime;
    QString m_syncStatus;

    // Supply Chain Data (in-memory caches of the database tables)

    // Counters
    int m_nextPONumber;
    int m_nextGRNNumber;
    int m_nextIssueNumber;
    int m_nextDCNumber;
    int m_nextPRNumber;

    // Supply Chain file paths
    QString m_dataDir;
    QTimer m_autoSaveTimer;
    QTimer m_cloudPollTimer;
    QTimer m_liveSyncTimer;   // polls the shared DB for other machines' edits

    // Existing helpers
    void setUnsavedChanges(bool changed);
    QString cleanFilePath(const QString &path);
    void recalculateSerialNumbers();
    void scheduleAutoSave();
    void savePermanentFileSettings();
    void loadPermanentFileSettings();
    void updateAutoSyncState();
    void initializeUploadsDirectory();
    int findPartByName(const QString &partName);
    bool updateExistingPart(int row, const QVector<QVariant> &newData);

    // Cloud helpers
    void updateSyncStatus(const QString &status);
    void saveCloudSettings();
    void loadCloudSettings();
    bool isFileLocked(const QString &filePath) const;
    bool lockFile(const QString &filePath);
    bool unlockFile(const QString &filePath);

    // Supply Chain helpers
    void initializeDataDirectory();

    // Stock grid persistence in the shared database.
    // Recomputes a challan header's line count, total quantity and row label
    // from its lines.
    // Writes one challan's lines, replacing whatever it had before.
    // Recomputes a request header's line count, quantity, estimated value and
    // row label from its lines.
    // Writes one request's lines, replacing whatever it had before.
    void loadSupplyChainCounters();
    void saveSupplyChainCounters();
};

#endif // EXCELHANDLER_H
