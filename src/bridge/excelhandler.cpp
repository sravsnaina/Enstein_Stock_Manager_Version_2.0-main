#include "bridge/excelhandler.h"

#include <QSet>
#include <QMap>
#include <algorithm>
#include "documents/podocument.h"
#include "documents/dcdocument.h"
#include "core/dbmanager.h"
#include "core/serversetup.h"
#include "core/dbschema.h"
#include "core/appsettings.h"
#include "domain/vendorservice.h"
#include "domain/itemmasterservice.h"
#include "domain/deliverychallanservice.h"
#include "domain/purchaserequestservice.h"
#include "domain/stockmovementservice.h"
#include "domain/stockservice.h"
#include "domain/materialissueservice.h"
#include "domain/purchaseorderservice.h"
#include "domain/goodsreceiptservice.h"
#include "domain/lowstockservice.h"
#include <QFileDialog>
#include <QtMath>

// QXlsx is an implementation detail of the .xlsx import and export
// paths; it is deliberately not named in the header, so nothing that
// includes the bridge needs QXlsx on its include path.
#include <xlsxdocument.h>
#include <xlsxworksheet.h>

static int toStockInt(const QVariant &value)
{
    bool ok = false;
    double number = value.toDouble(&ok);
    if (!ok) {
        number = value.toString().trimmed().toDouble(&ok);
        if (!ok) return 0;
    }
    return qRound(number);
}

// ==================== ExcelHandler Implementation ====================

ExcelHandler::~ExcelHandler()
{
    // Counters and Session are not QObjects, so they are not cleaned up by the
    // parent-child ownership that frees the services.
    delete m_counters;
    delete m_session;
}

ExcelHandler::ExcelHandler(QObject *parent)
    : QObject(parent),
    m_db(new DatabaseManager(this)),
    m_serverSetup(new ServerSetup(this)),
    m_model(new ExcelTableModel(this)),
    m_counters(new Counters(m_db)),
    m_session(new Session),
    m_vendors(nullptr),
    m_items(nullptr),
    m_challans(nullptr),
    m_requests(nullptr),
    m_movements(nullptr),
    m_stock(nullptr),
    m_issues(nullptr),
    m_orders(nullptr),
    m_grn(nullptr),
    m_lowStock(nullptr),
    m_hasUnsavedChanges(false),
    m_syncEnabled(false),
    m_syncStatus("offline"),
    m_nextPONumber(1),
    m_nextGRNNumber(1),
    m_nextIssueNumber(1),
    m_nextDCNumber(1),
    m_nextPRNumber(1)
{
    // Everything shared that no single service owns: the database, the
    // document numbering and who is logged in.
    const AppContext ctx{m_db, m_counters, m_session, m_model};
    m_vendors  = new VendorService(ctx, this);
    m_items    = new ItemMasterService(ctx, this);
    m_challans = new DeliveryChallanService(ctx, m_items, this);
    m_requests = new PurchaseRequestService(ctx, m_items, this);
    m_movements = new StockMovementService(ctx, this);
    m_stock = new StockService(ctx, this);
    m_issues = new MaterialIssueService(ctx, m_stock, m_movements, this);
    m_orders = new PurchaseOrderService(ctx, m_items, m_vendors, m_movements, this);
    m_grn = new GoodsReceiptService(ctx, m_orders, m_stock, m_movements, this);
    m_lowStock = new LowStockService(ctx, m_items, m_stock, m_orders, m_vendors, this);

    connect(m_model, &QAbstractItemModel::dataChanged,
            this, &ExcelHandler::onModelDataChanged);
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, &ExcelHandler::onModelDataChanged);
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, &ExcelHandler::onModelDataChanged);

    // Domain services report in their own vocabulary; the bridge translates
    // that onto the signal names QML already binds to.
    connect(m_vendors, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_vendors, &VendorService::listChanged,
            this, &ExcelHandler::vendorListChanged);

    connect(m_items, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    // One service signal, two QML signals: the low-stock badge is derived from
    // the item master's required quantities, so it goes stale with the list.
    connect(m_items, &ItemMasterService::listChanged, this, [this] {
        emit itemMasterListChanged();
        emit lowStockCountChanged();
    });

    connect(m_challans, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_challans, &DeliveryChallanService::listChanged,
            this, &ExcelHandler::deliveryChallanListChanged);
    connect(m_challans, &DeliveryChallanService::created,
            this, &ExcelHandler::deliveryChallanCreated);

    connect(m_requests, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_requests, &PurchaseRequestService::listChanged,
            this, &ExcelHandler::purchaseRequestListChanged);
    connect(m_requests, &PurchaseRequestService::created,
            this, &ExcelHandler::purchaseRequestCreated);

    connect(m_movements, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_movements, &StockMovementService::logged,
            this, &ExcelHandler::movementLogged);

    connect(m_stock, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_stock, &StockService::loaded, this, [this] {
        setUnsavedChanges(false);
    });
    connect(m_stock, &StockService::remoteChangesDetected, this, [this] {
        refreshFromDatabase();
        emit sharedDataChanged();
    });

    connect(m_issues, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_issues, &MaterialIssueService::issued,
            this, &ExcelHandler::stockIssued);

    connect(m_orders, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    connect(m_orders, &PurchaseOrderService::created,
            this, &ExcelHandler::purchaseOrderCreated);
    // Open orders count towards covering a shortage, so the low-stock badge
    // moves whenever the order list does.
    connect(m_orders, &PurchaseOrderService::listChanged, this, [this] {
        emit pendingPOCountChanged();
        emit lowStockCountChanged();
    });

    connect(m_grn, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    // Receiving moves three things at once: the order is closer to complete,
    // stock went up, and the shortage list is shorter.
    connect(m_grn, &GoodsReceiptService::received,
            this, [this](const QString &grnNo, const QString &poNo) {
        emit goodsReceived(grnNo, poNo);
        emit pendingPOCountChanged();
        emit lowStockCountChanged();
    });

    connect(m_lowStock, &Service::errorOccurred,
            this, &ExcelHandler::errorOccurred);
    // Quantities moved: mirror the grid to the permanent workbook if one is
    // set, and let the low-stock badge know it is stale.
    connect(m_stock, &StockService::changed, this, [this] {
        if (!m_permanentFile.isEmpty()) saveToPermanent();
        emit lowStockCountChanged();
    });

    m_autoSaveTimer.setSingleShot(true);
    connect(&m_autoSaveTimer, &QTimer::timeout,
            this, &ExcelHandler::autoSavePermanent);

    // Watches the shared database so an edit made on any other machine appears
    // here on its own. Only meaningful against a server; a local file has no
    // other writers, so the timer stays idle in that case.
    m_liveSyncTimer.setInterval(4000);
    m_liveSyncTimer.setSingleShot(false);
    connect(&m_liveSyncTimer, &QTimer::timeout, this, [this]() {
        if (!m_db || !m_db->isConnected() || !m_db->isServerBackend()) return;
        // A grid edit is still queued for saving; reloading now would replace it
        // with the server's older copy. Let the save land and pick this up on
        // the next tick instead.
        if (m_autoSaveTimer.isActive()) return;
        if (!m_db->hasRemoteChanges()) return;
        refreshFromDatabase();          // also re-marks the version
        emit sharedDataChanged();
    });

    m_cloudPollTimer.setInterval(10000);
    m_cloudPollTimer.setSingleShot(false);
    connect(&m_cloudPollTimer, &QTimer::timeout,
            this, &ExcelHandler::autoSyncFromCloud);

    initializeUploadsDirectory();
    initializeDataDirectory();
    loadPermanentFileSettings();
    loadCloudSettings();

    // Storage now lives in a shared SQL database instead of a Dropbox folder,
    // so the old folder-based cloud sync stays disabled.
    m_syncEnabled = false;

    // Connect to the shared database (Postgres in production, local SQLite
    // fallback otherwise) and build the schema before loading any data.
    if (!m_db->connectDatabase()) {
        emit errorOccurred("Could not open the database: " + m_db->lastError());
    }
    connect(m_db, &DatabaseManager::databaseError,
            this, [this](const QString &msg) { emit errorOccurred(msg); });

    connect(m_serverSetup, &ServerSetup::progress,
            this, &ExcelHandler::serverProvisionProgress);
    connect(m_serverSetup, &ServerSetup::finished, this,
            [this](bool success, const QVariantMap &result) {
                if (success) {
                    // Reconnect this machine itself to the shared database
                    // it just provisioned, using the exact same client path
                    // every other workstation will use.
                    configureDatabase("QPSQL", "localhost", result.value("port").toInt(),
                                       result.value("name").toString(),
                                       result.value("user").toString(),
                                       result.value("password").toString());
                }
                emit serverProvisionFinished(success, result);
            });

    // Load supply chain data from the database
    // Start watching for other machines' edits when this is a shared server.
    if (m_db->isConnected() && m_db->isServerBackend()) {
        m_db->syncVersionMark();
        m_liveSyncTimer.start();
    }

    m_vendors->load();
    m_items->load();
    m_orders->load();
    m_movements->load();
    m_issues->load();
    m_grn->load();
    m_challans->load();
    m_requests->load();

    // The stock grid also lives in the shared database. Fall back to an
    // empty default grid on a fresh database (no permanent file needed).
    if (!m_stock->loadFromDb())
        createStockFile(15);

    qDebug() << "ExcelHandler created with Supply Chain support (DB backend:"
             << m_db->backendName() << ")";
    qDebug() << "Vendors:" << m_vendors->count()
             << "| POs:" << m_orders->orders().size()
             << "| Movements:" << m_movements->count();
}

// ==================== Data Directory ====================

void ExcelHandler::initializeDataDirectory()
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_dataDir = appData + "/supplychain";

    QDir dir;
    if (!dir.exists(m_dataDir)) {
        dir.mkpath(m_dataDir);
        qDebug() << "Created supply chain data dir:" << m_dataDir;
    }
}

void ExcelHandler::initializeUploadsDirectory()
{
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_uploadsDir = appData + "/uploads";
    QDir dir;
    if (!dir.exists(m_uploadsDir)) {
        dir.mkpath(m_uploadsDir);
    }
}

// ==================== Supply Chain Counters ====================

void ExcelHandler::loadSupplyChainCounters() { m_counters->load(); }
void ExcelHandler::saveSupplyChainCounters() { m_counters->save(); }

// ==================== VENDOR MANAGEMENT ====================
// Delegated to VendorService. These stay here because they are part of the
// QML contract (docs/QML_API_REFERENCE.md) and their names cannot change
// without breaking the frontend.

int ExcelHandler::totalVendors() const                      { return m_vendors->count(); }
bool ExcelHandler::addVendorDetails(QVariantMap vendor)    { return m_vendors->add(vendor); }
bool ExcelHandler::updateVendorDetails(QVariantMap vendor) { return m_vendors->update(vendor); }
bool ExcelHandler::deleteVendor(const QString &name)       { return m_vendors->remove(name); }
QVariantList ExcelHandler::getVendorList()                 { return m_vendors->list(); }
QStringList ExcelHandler::getVendorNames()                 { return m_vendors->names(); }
QVariantMap ExcelHandler::getVendorByName(const QString &name) { return m_vendors->byName(name); }


// ===================== ITEM MASTER MANAGEMENT ======================
// Delegated to ItemMasterService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

bool ExcelHandler::addItemMasterDetails(QVariantMap d)    { return m_items->add(d); }
bool ExcelHandler::updateItemMasterDetails(QVariantMap d) { return m_items->update(d); }
QVariantList ExcelHandler::getItemMasterList()            { return m_items->list(); }
bool ExcelHandler::deleteItem(const QString &partName)    { return m_items->removeByPartName(partName); }


// ==================== PURCHASE ORDER MANAGEMENT ====================

// ==================== PURCHASE ORDER MANAGEMENT ====================
// Delegated to PurchaseOrderService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QString ExcelHandler::createPurchaseOrderItems(const QVariantList &items, const QString &expectedDate, const QString &expectedEndDate, const QString &preparedBy) { return m_orders->createWithItems(items, expectedDate, expectedEndDate, preparedBy); }
QString ExcelHandler::createPurchaseOrder(const QString &vendor, const QString &partName, const QString &partNo, int qty, double unitPrice, const QString &expectedDate, const QString &department, const QString &preparedBy, const QString &expectedEndDate) { return m_orders->create(vendor, partName, partNo, qty, unitPrice, expectedDate, department, preparedBy, expectedEndDate); }
QVariantList ExcelHandler::getPOItems(const QString &poNo)   { return m_orders->itemsFor(poNo); }
bool ExcelHandler::sendPOForApproval(const QString &poNo, const QString &approvedBy) { return m_orders->sendForApproval(poNo, approvedBy); }
QVariantList ExcelHandler::getPOList(const QString &statusFilter) { return m_orders->list(statusFilter); }
QVariantMap ExcelHandler::getPOSearchIndex() const           { return m_orders->searchIndex(); }
bool ExcelHandler::updatePOStatus(const QString &poNo, const QString &s) { return m_orders->setStatus(poNo, s); }
bool ExcelHandler::updatePurchaseOrder(const QString &poNo, const QVariantMap &d) { return m_orders->update(poNo, d); }
QVariantMap ExcelHandler::getPOByNumber(const QString &poNo) { return m_orders->byNumber(poNo); }
QString ExcelHandler::getNextPONumber()                      { return m_orders->nextNumber(); }
int ExcelHandler::pendingPOCount() const                     { return m_orders->pendingCount(); }
QVariantMap ExcelHandler::generatePOPreview(const QString &poNo, const QString &comments) { return m_orders->generatePreview(poNo, comments); }
QString ExcelHandler::savePOPdf(const QString &poNo, const QString &comments, const QString &destPath) { return m_orders->savePdf(poNo, comments, destPath); }
QString ExcelHandler::defaultPOPdfPath(const QString &poNo) const { return m_orders->defaultPdfPath(poNo); }

// ==================== STOCK GRID <-> DATABASE ====================
bool ExcelHandler::importStockFile(const QString &filePath)
{
    // Loads the user's stock xlsx (with the usual normalisation and legacy
    // column migration) and stores it as the shared stock in the database.
    if (!loadExcel(filePath))
        return false;

    m_stock->saveToDb();
    setUnsavedChanges(false);
    emit lowStockCountChanged();
    qDebug() << "Imported stock file into database:" << filePath;
    return true;
}

// ===================== PRINTABLE PURCHASE ORDER ======================

// The company's own details, printed on every document. Stored in QSettings
// by core/appsettings; these two are here only because QML asks for them.
QVariantMap ExcelHandler::getCompanyProfile() const         { return loadCompanyProfile(); }
bool ExcelHandler::saveCompanyProfile(const QVariantMap &p) { return ::saveCompanyProfile(p); }


bool ExcelHandler::openInSystemViewer(const QString &path)
{
    QString local = path.trimmed();
    if (local.startsWith("file://"))
        local = QUrl(local).toLocalFile();
    if (!QFile::exists(local))
        return false;
    return QDesktopServices::openUrl(QUrl::fromLocalFile(local));
}

// ==================== DELIVERY CHALLAN ====================
// Delegated to DeliveryChallanService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QString ExcelHandler::createDeliveryChallan(const QVariantMap &c, const QVariantList &i) { return m_challans->create(c, i); }
bool ExcelHandler::updateDeliveryChallan(const QString &dcNo, const QVariantMap &c, const QVariantList &i) { return m_challans->update(dcNo, c, i); }
bool ExcelHandler::deleteDeliveryChallan(const QString &dcNo)  { return m_challans->remove(dcNo); }
bool ExcelHandler::updateDCStatus(const QString &dcNo, const QString &s) { return m_challans->setStatus(dcNo, s); }
QVariantList ExcelHandler::getDCList(const QString &statusFilter) { return m_challans->list(statusFilter); }
QVariantMap ExcelHandler::getDCSearchIndex() const             { return m_challans->searchIndex(); }
QVariantList ExcelHandler::getDCItems(const QString &dcNo)     { return m_challans->itemsFor(dcNo); }
QVariantMap ExcelHandler::getDCByNumber(const QString &dcNo)   { return m_challans->byNumber(dcNo); }
QString ExcelHandler::getNextDCNumber()                        { return m_challans->nextNumber(); }
QVariantList ExcelHandler::getDCPartyList() const              { return m_challans->partyList(); }
QVariantMap ExcelHandler::generateDCPreview(const QString &dcNo) { return m_challans->generatePreview(dcNo); }
QString ExcelHandler::saveDCPdf(const QString &dcNo, const QString &destPath) { return m_challans->savePdf(dcNo, destPath); }
QString ExcelHandler::defaultDCPdfPath(const QString &dcNo) const { return m_challans->defaultPdfPath(dcNo); }


// ==================== PURCHASE REQUEST ====================
// Delegated to PurchaseRequestService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QString ExcelHandler::createPurchaseRequest(const QVariantMap &r, const QVariantList &i) { return m_requests->create(r, i); }
bool ExcelHandler::updatePurchaseRequest(const QString &prNo, const QVariantMap &r, const QVariantList &i) { return m_requests->update(prNo, r, i); }
bool ExcelHandler::deletePurchaseRequest(const QString &prNo) { return m_requests->remove(prNo); }
bool ExcelHandler::setPurchaseRequestStatus(const QString &prNo, const QString &s, const QString &by, const QString &note) { return m_requests->setStatus(prNo, s, by, note); }
bool ExcelHandler::linkRequestToPO(const QString &prNo, const QString &poNo) { return m_requests->linkToPO(prNo, poNo); }
QVariantList ExcelHandler::getPRList(const QString &statusFilter) { return m_requests->list(statusFilter); }
QVariantMap ExcelHandler::getPRSearchIndex() const           { return m_requests->searchIndex(); }
QVariantList ExcelHandler::getPRItems(const QString &prNo)   { return m_requests->itemsFor(prNo); }
QVariantMap ExcelHandler::getPRByNumber(const QString &prNo) { return m_requests->byNumber(prNo); }
QString ExcelHandler::getNextPRNumber()                      { return m_requests->nextNumber(); }
int ExcelHandler::pendingRequestCount() const                { return m_requests->pendingCount(); }


// ==================== GOODS RECEIPT NOTE (GRN) ====================
// Delegated to GoodsReceiptService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QString ExcelHandler::receiveGoodsForItem(int itemId, int receivedQty, int acceptedQty,
                                          int rejectedQty, const QString &remarks,
                                          const QString &receivedBy)
{ return m_grn->receiveForItem(itemId, receivedQty, acceptedQty, rejectedQty, remarks, receivedBy); }

QString ExcelHandler::receiveGoods(const QString &poNo, int receivedQty, int acceptedQty,
                                   int rejectedQty, const QString &remarks,
                                   const QString &receivedBy)
{ return m_grn->receiveForOrder(poNo, receivedQty, acceptedQty, rejectedQty, remarks, receivedBy); }

QVariantList ExcelHandler::getGRNList()                { return m_grn->list(); }

// ==================== STOCK MOVEMENT LOG ====================
// Delegated to StockMovementService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

bool ExcelHandler::logStockMovement(const QString &partName, const QString &partNo,
                                    const QString &movementType, int qty,
                                    const QString &reference, const QString &doneBy)
{ return m_movements->log(partName, partNo, movementType, qty, reference, doneBy); }
QVariantList ExcelHandler::getStockMovements(const QString &partNameFilter) { return m_movements->forPart(partNameFilter); }
QVariantList ExcelHandler::getAllMovements()                 { return m_movements->all(); }


// ==================== MATERIAL ISSUE ====================
// Delegated to MaterialIssueService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QString ExcelHandler::issueMultipleStock(const QVariantList &items, const QString &department, const QString &issuedBy) { return m_issues->issueMany(items, department, issuedBy); }
QString ExcelHandler::issueStock(const QString &partName, int qty, const QString &department, const QString &issuedBy) { return m_issues->issueOne(partName, qty, department, issuedBy); }
QVariantList ExcelHandler::getIssueNotes()                   { return m_issues->list(); }


// ==================== LOW STOCK ALERTS ====================
// Delegated to LowStockService. Part of the QML contract; see
// docs/QML_API_REFERENCE.md.

QVariantList ExcelHandler::getLowStockItems()          { return m_lowStock->items(); }
int ExcelHandler::lowStockCount()                      { return m_lowStock->count(); }
bool ExcelHandler::autoGeneratePOForLowStock()         { return m_lowStock->autoGeneratePO(); }

// ==================== Helper: Find Part Row ====================


// ==================== EXISTING METHODS (kept intact with column adjustments) ====================

void ExcelHandler::savePermanentFileSettings()
{
    QSettings settings("EinsteinRobotics", "StockManager");
    settings.setValue("permanentFile", m_permanentFile);
}

void ExcelHandler::loadPermanentFileSettings()
{
    QSettings settings("EinsteinRobotics", "StockManager");
    QString savedPath = settings.value("permanentFile", "").toString();

    if (!savedPath.isEmpty() && QFileInfo::exists(savedPath)) {
        QFileInfo fileInfo(savedPath);
        QString suffix = fileInfo.suffix().toLower();
        if (suffix == "xlsx" || suffix == "xls") {
            m_permanentFile = savedPath;
            emit permanentFileChanged();
        } else {
            m_permanentFile = "";
            savePermanentFileSettings();
        }
    }
}

QString ExcelHandler::getSavedPermanentPath()
{
    return m_permanentFile;
}

bool ExcelHandler::hasSavedPermanentFile()
{
    return !m_permanentFile.isEmpty() && QFileInfo::exists(m_permanentFile);
}

bool ExcelHandler::setPermanentFile(const QString &filePath)
{
    QString cleanPath = cleanFilePath(filePath);
    QFileInfo fileInfo(cleanPath);

    if (!fileInfo.exists()) {
        emit errorOccurred("File does not exist: " + cleanPath);
        return false;
    }

    QString suffix = fileInfo.suffix().toLower();
    if (suffix != "xlsx" && suffix != "xls") {
        emit errorOccurred("Permanent file must be Excel format (.xlsx or .xls)");
        return false;
    }

    m_permanentFile = cleanPath;
    m_currentFile = cleanPath;
    savePermanentFileSettings();
    emit permanentFileChanged();
    emit currentFileChanged();
    updateAutoSyncState();

    return true;
}

bool ExcelHandler::loadPermanentFile()
{
    if (m_permanentFile.isEmpty()) {
        emit errorOccurred("No permanent file set");
        return false;
    }

    QFileInfo fileInfo(m_permanentFile);
    if (!fileInfo.exists()) {
        m_permanentFile = "";
        savePermanentFileSettings();
        emit permanentFileChanged();
        emit errorOccurred("Permanent file not found");
        return false;
    }

    return loadExcel(m_permanentFile);
}

bool ExcelHandler::saveToPermanent()
{
    if (m_permanentFile.isEmpty()) {
        emit errorOccurred("No permanent file set");
        return false;
    }
    return saveExcel(m_permanentFile);
}

int ExcelHandler::findPartByName(const QString &partName)
{
    return m_stock->findRowByName(partName);
}

bool ExcelHandler::updateExistingPart(int row, const QVector<QVariant> &mergeData)
{
    int currentStock = toStockInt(m_model->getData(row, 2));
    int purchaseQty = mergeData.size() > 2 ? toStockInt(mergeData[2]) : 0;
    int newStock = currentStock + purchaseQty;

    m_model->setDataAt(row, 2, newStock);

    if (mergeData.size() > 1 && !mergeData[1].toString().trimmed().isEmpty())
        m_model->setDataAt(row, 1, mergeData[1]);
    if (mergeData.size() > 3 && !mergeData[3].toString().trimmed().isEmpty())
        m_model->setDataAt(row, 3, mergeData[3]);
    if (mergeData.size() > 4 && !mergeData[4].toString().trimmed().isEmpty())
        m_model->setDataAt(row, 4, mergeData[4]);
    if (mergeData.size() > 5 && !mergeData[5].toString().trimmed().isEmpty())
        m_model->setDataAt(row, 5, mergeData[5]);
    if (mergeData.size() > 6 && !mergeData[6].toString().trimmed().isEmpty())
        m_model->setDataAt(row, 6, mergeData[6]);

    return true;
}

bool ExcelHandler::appendFromFile(const QString &filePath)
{
    QString cleanPath = cleanFilePath(filePath);

    if (!validateFileStructure(cleanPath)) {
        emit errorOccurred("Only PURCHASE files can be merged into Stock files.");
        return false;
    }

    QXlsx::Document xlsx(cleanPath);
    if (!xlsx.load()) {
        emit errorOccurred("Failed to load file");
        return false;
    }

    QXlsx::Worksheet *sheet = xlsx.currentWorksheet();
    if (!sheet) {
        emit errorOccurred("No worksheet found");
        return false;
    }

    QXlsx::CellRange range = sheet->dimension();
    int rowsAdded = 0, rowsUpdated = 0;
    QSet<QString> processedParts;

    for (int row = 2; row <= range.lastRow(); ++row) {
        QVector<QVariant> rowData;
        for (int col = 1; col <= 7; ++col) {
            auto cell = sheet->cellAt(row, col);
            rowData.append(cell ? cell->value() : QVariant());
        }

        QString partName = rowData.size() > 0 ? rowData[0].toString().trimmed() : "";
        if (partName.isEmpty()) continue;

        QString partNameLower = partName.toLower();
        if (processedParts.contains(partNameLower)) continue;
        processedParts.insert(partNameLower);

        int existingRow = findPartByName(partName);

        if (existingRow != -1) {
            updateExistingPart(existingRow, rowData);
            rowsUpdated++;
        } else {
            m_model->addRow();
            int newRow = m_model->rowCount() - 1;
            int purchaseQty = rowData.size() > 2 ? toStockInt(rowData[2]) : 0;

            m_model->setDataAt(newRow, 0, partName);
            m_model->setDataAt(newRow, 1, rowData.size() > 1 ? rowData[1] : QVariant());
            m_model->setDataAt(newRow, 2, purchaseQty);
            m_model->setDataAt(newRow, 3, rowData.size() > 3 ? rowData[3] : QVariant());
            m_model->setDataAt(newRow, 4, rowData.size() > 4 ? rowData[4] : QVariant());
            m_model->setDataAt(newRow, 5, rowData.size() > 5 ? rowData[5] : QVariant());
            m_model->setDataAt(newRow, 6, rowData.size() > 6 ? rowData[6] : QVariant());
            rowsAdded++;
        }

        // Log the movement
        QString partNo = rowData.size() > 1 ? rowData[1].toString() : "";
        int qty = rowData.size() > 2 ? toStockInt(rowData[2]) : 0;
        logStockMovement(partName, partNo, "IN", qty, "Merge: " + QFileInfo(cleanPath).fileName(), m_session->currentUser());
    }

    if (!m_permanentFile.isEmpty()) {
        saveToPermanent();
    }

    emit fileMerged(QFileInfo(cleanPath).fileName(), rowsAdded, rowsUpdated);
    emit lowStockCountChanged();
    return true;
}

bool ExcelHandler::appendStockFile(const QString &filePath)
{
    QString cleanPath = cleanFilePath(filePath);

    QXlsx::Document xlsx(cleanPath);
    if (!xlsx.load()) {
        emit errorOccurred("Failed to load file");
        return false;
    }

    QXlsx::Worksheet *sheet = xlsx.currentWorksheet();
    if (!sheet) {
        emit errorOccurred("No worksheet found");
        return false;
    }

    QXlsx::CellRange range = sheet->dimension();
    int startRow = qMax(range.firstRow(), 1);
    int endRow = qMax(range.lastRow(), 1);
    int startCol = qMax(range.firstColumn(), 1);
    int endCol = qMin(qMax(range.lastColumn(), 1), 26);

    QVector<QVector<QVariant>> data;
    int consecutiveEmptyRows = 0;

    for (int row = startRow; row <= endRow; ++row) {
        QVector<QVariant> rowData;
        bool rowIsEmpty = true;
        int actualColumns = 0;

        for (int col = startCol; col <= endCol; ++col) {
            auto cell = sheet->cellAt(row, col);
            QVariant cellValue;
            if (cell && cell->value().isValid()) {
                cellValue = cell->value();
                if (!cellValue.toString().trimmed().isEmpty()) {
                    rowIsEmpty = false;
                    actualColumns = qMax(actualColumns, col - startCol + 1);
                }
            }
            rowData.append(cellValue);
        }

        if (row == startRow && !rowIsEmpty) {
            while (rowData.size() > actualColumns) rowData.removeLast();
        }

        if (rowIsEmpty && row > startRow) {
            consecutiveEmptyRows++;
            if (consecutiveEmptyRows >= 5) break;
            continue;
        } else if (!rowIsEmpty) {
            consecutiveEmptyRows = 0;
            if (row > startRow && !data.isEmpty()) {
                int headerCols = data[0].size();
                while (rowData.size() > headerCols) rowData.removeLast();
                while (rowData.size() < headerCols) rowData.append(QVariant());
            }
            data.append(rowData);
        }
    }

    if (!data.isEmpty()) {
        int maxCols = data[0].size();
        for (auto &row : data) {
            while (row.size() < maxCols) row.append(QVariant());
            while (row.size() > maxCols) row.removeLast();
        }
    }

    if (data.isEmpty()) {
        emit errorOccurred("No data found in Excel file");
        return false;
    }

    if (data[0].size() < 3 ||
        data[0][2].toString().trimmed().toLower() != "stock") {
        emit errorOccurred("Only STOCK files can be appended");
        return false;
    }

    // Normalize stock sheets to current 9-column layout.
    {
        auto normalizeHeader = [](const QVariant &value) {
            return value.toString().trimmed().toLower();
        };

        QHash<QString, int> headerColumns;
        for (int col = 0; col < data[0].size(); ++col) {
            QString header = normalizeHeader(data[0][col]);
            if (!header.isEmpty()) {
                headerColumns.insert(header, col);
            }
        }

        bool hasLegacyReorderColumns = headerColumns.contains("min stock") ||
                                       headerColumns.contains("reorder level") ||
                                       headerColumns.contains("reorder lvl") ||
                                       headerColumns.contains("reorder");
        bool hasDateColumn = headerColumns.contains("date");
        bool needsNormalization = data[0].size() != 9 || hasLegacyReorderColumns || !hasDateColumn;

        if (needsNormalization) {
            auto colIndex = [&headerColumns](const QString &header, int fallback) {
                return headerColumns.contains(header) ? headerColumns.value(header) : fallback;
            };
            auto cellValue = [](const QVector<QVariant> &row, int col) -> QVariant {
                if (col < 0 || col >= row.size()) return QVariant();
                return row[col];
            };

            int colPartName = colIndex("part name", 0);
            int colPartNo = colIndex("part no", 1);
            int colStock = colIndex("stock", 2);
            int colDepartment = colIndex("department", 3);
            int colPrepared = colIndex("prepared", 4);
            int colApproved = colIndex("approved", 5);
            int colVendor = headerColumns.contains("vendor name")
                                ? headerColumns.value("vendor name")
                                : colIndex("vendor", 6);
            int colDate = colIndex("date", -1);
            int colUnitPrice = colIndex("unit price", data[0].size() > 9 ? 9 : 8);

            QVector<QVector<QVariant>> normalized;
            normalized.reserve(data.size());
            normalized.append({"Part Name", "Part No", "Stock",
                               "Department", "Prepared", "Approved", "Vendor Name",
                               "Date", "Unit Price"});

            for (int row = 1; row < data.size(); ++row) {
                const QVector<QVariant> &sourceRow = data[row];
                QVector<QVariant> targetRow(9);
                targetRow[0] = cellValue(sourceRow, colPartName);
                targetRow[1] = cellValue(sourceRow, colPartNo);
                targetRow[2] = cellValue(sourceRow, colStock);
                targetRow[3] = cellValue(sourceRow, colDepartment);
                targetRow[4] = cellValue(sourceRow, colPrepared);
                targetRow[5] = cellValue(sourceRow, colApproved);
                targetRow[6] = cellValue(sourceRow, colVendor);
                targetRow[7] = cellValue(sourceRow, colDate);
                targetRow[8] = cellValue(sourceRow, colUnitPrice);
                normalized.append(targetRow);
            }

            data = normalized;
        }
    }

    int rowsAdded = 0;
    int rowsUpdated = 0;

    for (int row = 1; row < data.size(); ++row) {
        const QVector<QVariant> &rowData = data[row];
        QString partName = rowData[0].toString().trimmed();
        if (partName.isEmpty()) continue;

        int existingRow = findPartByName(partName);
        if (existingRow != -1) {
            int currentStock = toStockInt(m_model->getData(existingRow, 2));
            int incomingStock = toStockInt(rowData[2]);
            m_model->setDataAt(existingRow, 2, currentStock + incomingStock);

            if (!rowData[1].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 1, rowData[1]);
            if (!rowData[3].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 3, rowData[3]);
            if (!rowData[4].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 4, rowData[4]);
            if (!rowData[5].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 5, rowData[5]);
            if (!rowData[6].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 6, rowData[6]);
            if (!rowData[7].toString().trimmed().isEmpty()) m_model->setDataAt(existingRow, 7, rowData[7]);
            if (rowData[8].isValid() && rowData[8].toDouble() > 0.0) m_model->setDataAt(existingRow, 8, rowData[8]);

            rowsUpdated++;
        } else {
            m_model->addRow();
            int newRow = m_model->rowCount() - 1;
            for (int col = 0; col < 9; ++col) {
                if (col == 2) {
                    m_model->setDataAt(newRow, col, toStockInt(rowData[col]));
                } else {
                    m_model->setDataAt(newRow, col, rowData[col]);
                }
            }
            rowsAdded++;
        }
    }

    if (!m_permanentFile.isEmpty()) {
        saveToPermanent();
    }

    emit fileMerged(QFileInfo(cleanPath).fileName(), rowsAdded, rowsUpdated);
    emit lowStockCountChanged();
    return true;
}

int ExcelHandler::searchPartName(const QString &partName)
{
    int row = findPartByName(partName);
    if (row != -1) emit searchResultFound(row);
    return row;
}

QVariantList ExcelHandler::searchAllMatches(const QString &searchText)
{
    QVariantList results;
    int rows = m_model->rowCount();
    QString search = searchText.trimmed().toLower();

    for (int row = 1; row < rows; ++row) {
        QString partName = m_model->getData(row, 0).toString().toLower();
        QString partNo = m_model->getData(row, 1).toString().toLower();
        QString vendor = m_model->getData(row, 6).toString().toLower();

        if (partName.contains(search) || partNo.contains(search) || vendor.contains(search)) {
            QVariantMap result;
            result["row"]      = row;
            result["partName"] = m_model->getData(row, 0);
            result["partNo"]   = m_model->getData(row, 1);
            result["stock"]    = m_model->getData(row, 2);
            result["vendor"]   = m_model->getData(row, 6);
            result["department"] = m_model->getData(row, 3);
            result["date"] = m_model->getData(row, 7);
            results.append(result);
        }
    }

    return results;
}

bool ExcelHandler::uploadFileForPart(int row, const QString &filePath)
{
    Q_UNUSED(row) Q_UNUSED(filePath)
    return false;
}

QString ExcelHandler::getUploadedFilePath(int row) { Q_UNUSED(row) return ""; }
bool ExcelHandler::openUploadedFile(int row) { Q_UNUSED(row) return false; }
bool ExcelHandler::hasUploadedFile(int row) { Q_UNUSED(row) return false; }

// ==================== CREATE FILES ====================

void ExcelHandler::createNew(int rows, int cols)
{
    Q_UNUSED(cols)

    QVector<QVector<QVariant>> data;
    data.reserve(rows);

    // 9 columns: Date added, Min Stock/Reorder removed
    QVector<QVariant> header = {"Part Name", "Part No", "Stock",
                                "Department", "Prepared", "Approved", "Vendor Name",
                                "Date", "Unit Price"};
    data.append(header);

    for (int i = 1; i < rows; ++i) {
        QVector<QVariant> row(9);
        data.append(row);
    }

    m_model->setExcelData(data);
    m_currentFile = "";
    setUnsavedChanges(false);
    emit currentFileChanged();
}

void ExcelHandler::createStockFile(int rows)
{
    QVector<QVector<QVariant>> data;
    data.reserve(rows);

    QVector<QVariant> header = {"Part Name", "Part No", "Stock",
                                "Department", "Prepared", "Approved", "Vendor Name",
                                "Date", "Unit Price"};
    data.append(header);

    for (int i = 1; i < rows; ++i) {
        QVector<QVariant> row(9);
        data.append(row);
    }

    m_model->setExcelData(data);
    m_currentFile = "";
    setUnsavedChanges(false);
    emit currentFileChanged();
    emit lowStockCountChanged();

    qDebug() << "Stock file created (9 columns with date and unit price)";
}

void ExcelHandler::createPurchaseFile(int rows)
{
    QVector<QVector<QVariant>> data;
    data.reserve(rows);

    QVector<QVariant> header = {"Part Name", "Part No", "Purchase",
                                "Department", "Prepared", "Approved", "Vendor Name"};
    data.append(header);

    for (int i = 1; i < rows; ++i) {
        QVector<QVariant> row(7);
        data.append(row);
    }

    m_model->setExcelData(data);
    m_currentFile = "";
    setUnsavedChanges(false);
    emit currentFileChanged();
}

QString ExcelHandler::getFileName() const
{
    if (m_currentFile.isEmpty()) return "Untitled";
    return QFileInfo(m_currentFile).fileName();
}

QString ExcelHandler::getFileType() const
{
    QVariant header = m_model->getData(0, 2);
    QString headerText = header.toString().trimmed().toLower();
    return (headerText == "purchase") ? "purchase" : "stock";
}

bool ExcelHandler::loadExcel(const QString &filePath)
{
    QString cleanPath = cleanFilePath(filePath);

    QFileInfo fileInfo(cleanPath);
    if (!fileInfo.exists()) {
        emit errorOccurred("File does not exist: " + cleanPath);
        return false;
    }

    QString suffix = fileInfo.suffix().toLower();
    if (suffix != "xlsx" && suffix != "xls") {
        emit errorOccurred("Please select an Excel file (.xlsx or .xls)");
        return false;
    }

    QXlsx::Document xlsx(cleanPath);
    if (!xlsx.load()) {
        emit errorOccurred("Failed to load Excel file");
        return false;
    }

    QXlsx::Worksheet *sheet = xlsx.currentWorksheet();
    if (!sheet) {
        emit errorOccurred("No worksheet found");
        return false;
    }

    QXlsx::CellRange range = sheet->dimension();
    int startRow = qMax(range.firstRow(), 1);
    int endRow = qMax(range.lastRow(), 1);
    int startCol = qMax(range.firstColumn(), 1);
    int endCol = qMin(qMax(range.lastColumn(), 1), 26);

    QVector<QVector<QVariant>> data;
    int consecutiveEmptyRows = 0;

    for (int row = startRow; row <= endRow; ++row) {
        QVector<QVariant> rowData;
        bool rowIsEmpty = true;
        int actualColumns = 0;

        for (int col = startCol; col <= endCol; ++col) {
            auto cell = sheet->cellAt(row, col);
            QVariant cellValue;
            if (cell && cell->value().isValid()) {
                cellValue = cell->value();
                if (!cellValue.toString().trimmed().isEmpty()) {
                    rowIsEmpty = false;
                    actualColumns = qMax(actualColumns, col - startCol + 1);
                }
            }
            rowData.append(cellValue);
        }

        if (row == startRow && !rowIsEmpty) {
            while (rowData.size() > actualColumns) rowData.removeLast();
        }

        if (rowIsEmpty && row > startRow) {
            consecutiveEmptyRows++;
            if (consecutiveEmptyRows >= 5) break;
            continue;
        } else if (!rowIsEmpty) {
            consecutiveEmptyRows = 0;
            if (row > startRow && !data.isEmpty()) {
                int headerCols = data[0].size();
                while (rowData.size() > headerCols) rowData.removeLast();
                while (rowData.size() < headerCols) rowData.append(QVariant());
            }
            data.append(rowData);
        }
    }

    if (!data.isEmpty()) {
        int maxCols = data[0].size();
        for (auto &row : data) {
            while (row.size() < maxCols) row.append(QVariant());
            while (row.size() > maxCols) row.removeLast();
        }
    }

    if (data.isEmpty()) {
        emit errorOccurred("No data found in Excel file");
        return false;
    }

    // Normalize stock sheets to current 9-column layout.
    // Older files may still use Min Stock / Reorder Level columns.
    if (!data.isEmpty() && data[0].size() >= 3) {
        auto normalizeHeader = [](const QVariant &value) {
            return value.toString().trimmed().toLower();
        };

        QString col3Header = normalizeHeader(data[0][2]);
        if (col3Header == "stock") {
            QHash<QString, int> headerColumns;
            for (int col = 0; col < data[0].size(); ++col) {
                QString header = normalizeHeader(data[0][col]);
                if (!header.isEmpty()) {
                    headerColumns.insert(header, col);
                }
            }

            bool hasLegacyReorderColumns = headerColumns.contains("min stock") ||
                                           headerColumns.contains("reorder level") ||
                                           headerColumns.contains("reorder lvl") ||
                                           headerColumns.contains("reorder");
            bool hasDateColumn = headerColumns.contains("date");
            bool needsNormalization = data[0].size() != 9 || hasLegacyReorderColumns || !hasDateColumn;

            if (needsNormalization) {
                auto colIndex = [&headerColumns](const QString &header, int fallback) {
                    return headerColumns.contains(header) ? headerColumns.value(header) : fallback;
                };
                auto cellValue = [](const QVector<QVariant> &row, int col) -> QVariant {
                    if (col < 0 || col >= row.size()) return QVariant();
                    return row[col];
                };

                int colPartName = colIndex("part name", 0);
                int colPartNo = colIndex("part no", 1);
                int colStock = colIndex("stock", 2);
                int colDepartment = colIndex("department", 3);
                int colPrepared = colIndex("prepared", 4);
                int colApproved = colIndex("approved", 5);
                int colVendor = headerColumns.contains("vendor name")
                                    ? headerColumns.value("vendor name")
                                    : colIndex("vendor", 6);
                int colDate = colIndex("date", -1);
                int colUnitPrice = colIndex("unit price", data[0].size() > 9 ? 9 : 8);

                QVector<QVector<QVariant>> normalized;
                normalized.reserve(data.size());
                normalized.append({"Part Name", "Part No", "Stock",
                                   "Department", "Prepared", "Approved", "Vendor Name",
                                   "Date", "Unit Price"});

                for (int row = 1; row < data.size(); ++row) {
                    const QVector<QVariant> &sourceRow = data[row];
                    QVector<QVariant> targetRow(9);
                    targetRow[0] = cellValue(sourceRow, colPartName);
                    targetRow[1] = cellValue(sourceRow, colPartNo);
                    targetRow[2] = cellValue(sourceRow, colStock);
                    targetRow[3] = cellValue(sourceRow, colDepartment);
                    targetRow[4] = cellValue(sourceRow, colPrepared);
                    targetRow[5] = cellValue(sourceRow, colApproved);
                    targetRow[6] = cellValue(sourceRow, colVendor);
                    targetRow[7] = cellValue(sourceRow, colDate);
                    targetRow[8] = cellValue(sourceRow, colUnitPrice);
                    normalized.append(targetRow);
                }

                data = normalized;
                qDebug() << "Normalized stock sheet to 9-column dashboard layout";
            }

            // Normalize stock column values to integers.
            for (int row = 1; row < data.size(); ++row) {
                if (data[row].size() > 2) {
                    data[row][2] = toStockInt(data[row][2]);
                }
            }
        }
    }

    m_model->setExcelData(data);
    m_currentFile = cleanPath;
    setUnsavedChanges(false);

    emit currentFileChanged();
    emit fileLoaded(fileInfo.fileName());
    emit lowStockCountChanged();

    qDebug() << "Loaded:" << data.size() << "rows x" << (data.isEmpty() ? 0 : data[0].size()) << "cols";
    return true;
}

bool ExcelHandler::saveExcel(const QString &filePath)
{
    QString savePath = filePath.isEmpty() ? m_currentFile : cleanFilePath(filePath);

    if (savePath.isEmpty()) {
        emit errorOccurred("No file path specified");
        return false;
    }

    if (!savePath.endsWith(".xlsx", Qt::CaseInsensitive) && !savePath.endsWith(".xls", Qt::CaseInsensitive)) {
        savePath += ".xlsx";
    }

    QXlsx::Document xlsx;
    QVector<QVector<QVariant>> data = m_model->getExcelData();

    for (int row = 0; row < data.size(); ++row) {
        for (int col = 0; col < data[row].size(); ++col) {
            xlsx.write(row + 1, col + 1, data[row][col]);
        }
    }

    if (!xlsx.saveAs(savePath)) {
        emit errorOccurred("Failed to save file");
        return false;
    }

    m_currentFile = savePath;
    setUnsavedChanges(false);
    emit currentFileChanged();
    emit fileSaved(QFileInfo(savePath).fileName());

    return true;
}

bool ExcelHandler::validateFileStructure(const QString &filePath)
{
    QString cleanPath = cleanFilePath(filePath);

    QXlsx::Document xlsx(cleanPath);
    if (!xlsx.load()) return false;

    QXlsx::Worksheet *sheet = xlsx.currentWorksheet();
    if (!sheet) return false;

    auto col3Cell = sheet->cellAt(1, 3);
    QString col3Header = col3Cell ? col3Cell->value().toString().trimmed().toLower() : "";

    return (col3Header == "purchase");
}

QString ExcelHandler::browseOpenFile(const QString &title, const QString &filter)
{
    return QFileDialog::getOpenFileName(nullptr, title, QString(), filter);
}

QString ExcelHandler::browseSaveFile(const QString &title, const QString &filter)
{
    return QFileDialog::getSaveFileName(nullptr, title, QString(), filter);
}

QString ExcelHandler::browseFolder(const QString &title)
{
    return QFileDialog::getExistingDirectory(nullptr, title, QString());
}

static QDateTime parseReportDate(const QString &value, const QTime &fallbackTime)
{
    QString text = value.trimmed();
    if (text.isEmpty()) return QDateTime();

    QDateTime dt = QDateTime::fromString(text, "yyyy-MM-dd hh:mm:ss");
    if (!dt.isValid()) dt = QDateTime::fromString(text, "yyyy-MM-dd hh:mm");
    if (!dt.isValid()) {
        QDate date = QDate::fromString(text, "yyyy-MM-dd");
        if (date.isValid()) {
            dt = QDateTime(date, fallbackTime);
        }
    }
    return dt;
}

bool ExcelHandler::exportReport(const QString &fromDate, const QString &toDate, const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        emit errorOccurred("Report file path is required");
        return false;
    }

    QDateTime fromDt = parseReportDate(fromDate, QTime(0, 0, 0));
    QDateTime toDt = parseReportDate(toDate, QTime(23, 59, 59));
    if (!fromDt.isValid()) fromDt = QDateTime::fromSecsSinceEpoch(0);
    if (!toDt.isValid()) toDt = QDateTime::currentDateTime();

    auto inRange = [&fromDt, &toDt](const QDateTime &dt) {
        if (!dt.isValid()) return false;
        return dt >= fromDt && dt <= toDt;
    };

    QString savePath = filePath;
    if (!savePath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        savePath += ".xlsx";
    }

    QXlsx::Document xlsx;
    xlsx.renameSheet("Sheet1", "StockMovements");
    xlsx.selectSheet("StockMovements");

    xlsx.write(1, 1, "Date");
    xlsx.write(1, 2, "Part Name");
    xlsx.write(1, 3, "Type");
    xlsx.write(1, 4, "Qty");
    xlsx.write(1, 5, "Reference");
    xlsx.write(1, 6, "Done By");

    int row = 2;
    for (const auto &mov : m_movements->rows()) {
        QDateTime dt = parseReportDate(mov.value("date").toString(), QTime(0, 0, 0));
        if (!inRange(dt)) continue;
        xlsx.write(row, 1, mov.value("date"));
        xlsx.write(row, 2, mov.value("partName"));
        xlsx.write(row, 3, mov.value("type"));
        xlsx.write(row, 4, mov.value("qty"));
        xlsx.write(row, 5, mov.value("reference"));
        xlsx.write(row, 6, mov.value("doneBy"));
        row++;
    }

    xlsx.addSheet("Issues");
    xlsx.selectSheet("Issues");
    xlsx.write(1, 1, "Date");
    xlsx.write(1, 2, "Issue No");
    xlsx.write(1, 3, "Part Name");
    xlsx.write(1, 4, "Qty");
    xlsx.write(1, 5, "Department");
    xlsx.write(1, 6, "Issued By");

    row = 2;
    for (const auto &note : m_issues->rows()) {
        QDateTime dt = parseReportDate(note.value("date").toString(), QTime(0, 0, 0));
        if (!inRange(dt)) continue;
        xlsx.write(row, 1, note.value("date"));
        xlsx.write(row, 2, note.value("issueNo"));
        xlsx.write(row, 3, note.value("partName"));
        xlsx.write(row, 4, note.value("qty"));
        xlsx.write(row, 5, note.value("department"));
        xlsx.write(row, 6, note.value("issuedBy"));
        row++;
    }

    xlsx.addSheet("GRN");
    xlsx.selectSheet("GRN");
    xlsx.write(1, 1, "Date");
    xlsx.write(1, 2, "GRN No");
    xlsx.write(1, 3, "PO No");
    xlsx.write(1, 4, "Part Name");
    xlsx.write(1, 5, "Received Qty");
    xlsx.write(1, 6, "Accepted Qty");
    xlsx.write(1, 7, "Rejected Qty");
    xlsx.write(1, 8, "Remarks");
    xlsx.write(1, 9, "Received By");

    row = 2;
    for (const auto &grn : m_grn->rows()) {
        QDateTime dt = parseReportDate(grn.value("date").toString(), QTime(0, 0, 0));
        if (!inRange(dt)) continue;
        xlsx.write(row, 1, grn.value("date"));
        xlsx.write(row, 2, grn.value("grnNo"));
        xlsx.write(row, 3, grn.value("poNo"));
        xlsx.write(row, 4, grn.value("partName"));
        xlsx.write(row, 5, grn.value("receivedQty"));
        xlsx.write(row, 6, grn.value("acceptedQty"));
        xlsx.write(row, 7, grn.value("rejectedQty"));
        xlsx.write(row, 8, grn.value("remarks"));
        xlsx.write(row, 9, grn.value("receivedBy"));
        row++;
    }

    xlsx.addSheet("PurchaseOrders");
    xlsx.selectSheet("PurchaseOrders");
    xlsx.write(1, 1, "PO No");
    xlsx.write(1, 2, "Date");
    xlsx.write(1, 3, "Vendor");
    xlsx.write(1, 4, "Part Name");
    xlsx.write(1, 5, "Part No");
    xlsx.write(1, 6, "Department");
    xlsx.write(1, 7, "Qty");
    xlsx.write(1, 8, "Unit Price");
    xlsx.write(1, 9, "Total Amount");
    xlsx.write(1, 10, "Expected From");
    xlsx.write(1, 11, "Expected To");
    xlsx.write(1, 12, "Status");
    xlsx.write(1, 13, "Received Qty");
    xlsx.write(1, 14, "Prepared By");
    xlsx.write(1, 15, "Approved By");
    xlsx.write(1, 16, "Received By");
    xlsx.write(1, 17, "Received Date");

    row = 2;
    for (const auto &po : m_orders->orders()) {
        QDateTime dt = parseReportDate(po.value("date").toString(), QTime(0, 0, 0));
        if (!inRange(dt)) continue;
        xlsx.write(row, 1, po.value("poNo"));
        xlsx.write(row, 2, po.value("date"));
        xlsx.write(row, 3, po.value("vendor"));
        xlsx.write(row, 4, po.value("partName"));
        xlsx.write(row, 5, po.value("partNo"));
        xlsx.write(row, 6, po.value("department"));
        xlsx.write(row, 7, po.value("qty"));
        xlsx.write(row, 8, po.value("unitPrice"));
        xlsx.write(row, 9, po.value("totalAmount"));
        xlsx.write(row, 10, po.value("expectedDate"));
        xlsx.write(row, 11, po.value("expectedEndDate"));
        xlsx.write(row, 12, po.value("status"));
        xlsx.write(row, 13, po.value("receivedQty"));
        xlsx.write(row, 14, po.value("preparedBy"));
        xlsx.write(row, 15, po.value("approvedBy"));
        xlsx.write(row, 16, po.value("receivedBy"));
        xlsx.write(row, 17, po.value("receivedDate"));
        row++;
    }

    if (!xlsx.saveAs(savePath)) {
        emit errorOccurred("Failed to save report");
        return false;
    }

    return true;
}

void ExcelHandler::addNewItem(const QString &partName, const QString &category,
                              int quantity, double unitPrice)
{
    int existingRow = findPartByName(partName);

    if (existingRow != -1) {
        int currentStock = toStockInt(m_model->getData(existingRow, 2));
        m_model->setDataAt(existingRow, 2, currentStock + quantity);

        // Log movement
        QString partNo = m_model->getData(existingRow, 1).toString();
        logStockMovement(partName, partNo, "IN", quantity, "Manual Add", m_session->currentUser());
    } else {
        m_model->addRow();
        int newRow = m_model->rowCount() - 1;

        m_model->setDataAt(newRow, 0, partName);
        m_model->setDataAt(newRow, 1, "PN-" + QString::number(newRow));
        m_model->setDataAt(newRow, 2, quantity);
        m_model->setDataAt(newRow, 3, category);
        m_model->setDataAt(newRow, 8, unitPrice);  // Column 8 = Unit Price

        logStockMovement(partName, "PN-" + QString::number(newRow), "IN", quantity, "New Item", m_session->currentUser());
    }

    emit lowStockCountChanged();
}

int ExcelHandler::getNextSerialNumber() const
{
    int maxNo = 0;
    int rows = m_model->rowCount();
    for (int row = 1; row < rows; ++row) {
        int no = m_model->getData(row, 0).toInt();
        if (no > maxNo) maxNo = no;
    }
    return maxNo + 1;
}

void ExcelHandler::onModelDataChanged()
{
    setUnsavedChanges(true);
    scheduleAutoSave();
}

void ExcelHandler::autoSavePermanent()
{
    // The database is the permanent store now.
    m_stock->saveToDb();
    setUnsavedChanges(false);

    // Legacy: also mirror to a permanent xlsx if one was configured.
    if (!m_permanentFile.isEmpty())
        saveToPermanent();
}

void ExcelHandler::autoSyncFromCloud()
{
    if (!m_syncEnabled || m_cloudFolder.isEmpty()) return;

    QString localFile = !m_currentFile.isEmpty() ? m_currentFile : m_permanentFile;
    if (localFile.isEmpty()) return;

    QString cloudFilePath = getCloudFilePath();
    if (cloudFilePath.isEmpty() || !QFile::exists(cloudFilePath)) return;
    if (isFileLocked(cloudFilePath)) return;

    QFileInfo localInfo(localFile);
    QFileInfo cloudInfo(cloudFilePath);

    if (cloudInfo.lastModified() > localInfo.lastModified()) {
        if (m_hasUnsavedChanges) {
            emit conflictDetected("Cloud file updated while local changes exist");
            return;
        }
        syncFromCloud();
    }
}

void ExcelHandler::setUnsavedChanges(bool changed)
{
    if (m_hasUnsavedChanges != changed) {
        m_hasUnsavedChanges = changed;
        emit unsavedChangesChanged();
    }
}

void ExcelHandler::scheduleAutoSave()
{
    // Debounced autosave into the shared database (and, if configured, the
    // legacy permanent xlsx file).
    m_autoSaveTimer.start(500);
}

void ExcelHandler::updateAutoSyncState()
{
    if (m_syncEnabled && !m_cloudFolder.isEmpty()) {
        if (!m_cloudPollTimer.isActive()) m_cloudPollTimer.start();
    } else {
        m_cloudPollTimer.stop();
    }
}

QString ExcelHandler::cleanFilePath(const QString &path)
{
    QString cleanPath = path;
    if (cleanPath.startsWith("file:///")) cleanPath = cleanPath.mid(8);
    else if (cleanPath.startsWith("file://")) cleanPath = cleanPath.mid(7);
#ifdef Q_OS_LINUX
    if (!cleanPath.isEmpty() && !cleanPath.startsWith('/')) cleanPath = "/" + cleanPath;
#endif
    return cleanPath;
}

void ExcelHandler::recalculateSerialNumbers() {}

// ==================== CLOUD SYNC (unchanged) ====================

void ExcelHandler::loadCloudSettings()
{
    QSettings settings("EinsteinRobotics", "StockManager");
    m_cloudFolder = settings.value("cloudFolder", "").toString();
    m_syncEnabled = settings.value("syncEnabled", false).toBool();
    m_session->setCurrentUser(settings.value("currentUser", "User").toString());
    m_session->setUserRole(settings.value("userRole", "editor").toString());
    m_syncStatus = "offline";
    m_lastSyncTime = settings.value("lastSyncTime", "Never").toString();

    if (!m_cloudFolder.isEmpty() && QDir(m_cloudFolder).exists()) {
        m_syncStatus = "synced";
    }

    emit cloudFolderChanged();
    emit syncEnabledChanged();
    emit currentUserChanged();
    emit userRoleChanged();
    emit lastSyncTimeChanged();
    emit syncStatusChanged();
}

void ExcelHandler::saveCloudSettings()
{
    QSettings settings("EinsteinRobotics", "StockManager");
    settings.setValue("cloudFolder", m_cloudFolder);
    settings.setValue("syncEnabled", m_syncEnabled);
    settings.setValue("currentUser", m_session->currentUser());
    settings.setValue("userRole", m_session->userRole());
    settings.setValue("lastSyncTime", m_lastSyncTime);
}

void ExcelHandler::setCloudFolder(const QString &folder)
{
    QString cleanPath = cleanFilePath(folder);
    QDir dir(cleanPath);
    if (!dir.exists()) {
        emit errorOccurred("Cloud folder does not exist: " + cleanPath);
        return;
    }
    m_cloudFolder = cleanPath;
    saveCloudSettings();
    emit cloudFolderChanged();
    updateAutoSyncState();
}

void ExcelHandler::setSyncEnabled(bool enabled)
{
    if (m_syncEnabled != enabled) {
        m_syncEnabled = enabled;
        saveCloudSettings();
        emit syncEnabledChanged();
        updateAutoSyncState();
    }
}

void ExcelHandler::setCurrentUser(const QString &username)
{
    if (m_session->currentUser() != username) {
        m_session->setCurrentUser(username);
        saveCloudSettings();
        emit currentUserChanged();
    }
}

void ExcelHandler::setUserRole(const QString &role)
{
    if (m_session->userRole() != role) {
        m_session->setUserRole(role);
        saveCloudSettings();
        emit userRoleChanged();
    }
}

void ExcelHandler::updateSyncStatus(const QString &status)
{
    if (m_syncStatus != status) {
        m_syncStatus = status;
        emit syncStatusChanged();
    }
}

bool ExcelHandler::canEdit() const
{
    return m_session->canEdit();
}

QString ExcelHandler::currentUser() const { return m_session->currentUser(); }
QString ExcelHandler::userRole() const    { return m_session->userRole(); }

QString ExcelHandler::getCloudFilePath() const
{
    QString sourceFile = !m_currentFile.isEmpty() ? m_currentFile : m_permanentFile;
    if (m_cloudFolder.isEmpty() || sourceFile.isEmpty()) return "";
    return m_cloudFolder + "/" + QFileInfo(sourceFile).fileName();
}

bool ExcelHandler::isFileLocked(const QString &filePath) const
{
    QString lockFile = filePath + ".lock";
    if (!QFile::exists(lockFile)) return false;

    QFileInfo lockInfo(lockFile);
    if (lockInfo.lastModified().secsTo(QDateTime::currentDateTime()) > 300) return false;

    QFile file(lockFile);
    if (file.open(QIODevice::ReadOnly)) {
        QString lockOwner = file.readAll().trimmed();
        file.close();
        return (lockOwner != m_session->currentUser());
    }
    return false;
}

bool ExcelHandler::lockFile(const QString &filePath)
{
    if (isFileLocked(filePath)) return false;
    QFile file(filePath + ".lock");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(m_session->currentUser().toUtf8());
        file.close();
        return true;
    }
    return false;
}

bool ExcelHandler::unlockFile(const QString &filePath)
{
    QString lockFile = filePath + ".lock";
    if (QFile::exists(lockFile)) {
        QFile::remove(lockFile);
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Dropbox / folder-based cloud sync is deprecated. All supply-chain data now
// lives in the shared SQL database, which handles multi-user concurrency
// natively, so these methods are intentionally no-ops kept only for binary /
// QML compatibility.
// ---------------------------------------------------------------------------
bool ExcelHandler::syncToCloud()
{
    updateSyncStatus("db");
    return false;
}

bool ExcelHandler::syncFromCloud()
{
    updateSyncStatus("db");
    return false;
}

bool ExcelHandler::checkForUpdates()
{
    return false;
}

// ==================== AUTHENTICATION ====================

QString ExcelHandler::login(const QString &username, const QString &password)
{
    if (!m_db) return QString();
    const QString role = m_db->authenticate(username, password);
    if (!role.isEmpty()) {
        setCurrentUser(username);
        setUserRole(role);
        qInfo() << "Login successful for" << username << "as" << role;
    }
    return role;
}

// ==================== DATABASE CONNECTION SETTINGS ====================

QVariantMap ExcelHandler::getDatabaseSettings() const
{
    return m_db ? m_db->connectionSettings() : QVariantMap();
}

QVariantMap ExcelHandler::copyLocalDataToServer()
{
    QVariantMap result;
    if (!m_db) {
        result["success"] = false;
        result["message"] = "No database connection";
        return result;
    }

    result = m_db->migrateLocalDataToServer();
    if (result.value("success").toBool()) {
        // Pick the newly shared rows up straight away.
        refreshFromDatabase();
    }
    return result;
}

bool ExcelHandler::configureDatabase(const QString &driver,
                                     const QString &host, int port,
                                     const QString &name,
                                     const QString &user, const QString &password)
{
    if (!m_db) return false;

    const bool ok = m_db->configureConnection(driver, host, port, name, user, password);
    if (ok) {
        // Reload every cache (including the stock grid) from the new database.
        refreshFromDatabase();
    }
    return ok;
}

QString ExcelHandler::databaseStatus() const
{
    if (!m_db) return "Not initialised";
    return (m_db->isConnected() ? "Connected: " : "Disconnected: ") + m_db->backendName();
}

bool ExcelHandler::isDatabaseConnected() const
{
    return m_db && m_db->isConnected();
}

bool ExcelHandler::isDatabaseServerBackend() const
{
    return m_db && m_db->isServerBackend();
}

QString ExcelHandler::databaseLastError() const
{
    return m_db ? m_db->lastError() : QString("Database not initialised");
}

// ==================== LAN SERVER PROVISIONING ====================

void ExcelHandler::setupThisComputerAsServer()
{
    if (m_serverSetup) m_serverSetup->provisionAsServer();
}

bool ExcelHandler::isServerProvisioned() const
{
    return m_serverSetup && m_serverSetup->isServerProvisioned();
}

QString ExcelHandler::serverLanAddressHint() const
{
    return m_serverSetup ? m_serverSetup->lanAddressHint() : QString();
}

void ExcelHandler::setLiveSyncInterval(int seconds)
{
    if (seconds <= 0) {
        m_liveSyncTimer.stop();
        return;
    }
    m_liveSyncTimer.setInterval(seconds * 1000);
    if (m_db && m_db->isConnected() && m_db->isServerBackend())
        m_liveSyncTimer.start();
}

int ExcelHandler::liveSyncInterval() const
{
    return m_liveSyncTimer.isActive() ? m_liveSyncTimer.interval() / 1000 : 0;
}

namespace {
// Column layout of the stock grid, as shown in the main window headers.
enum StockColumn { ColPartName = 0, ColPartNo = 1, ColQty = 2, ColDepartment = 3,
                   ColVendor = 6, ColUnitPrice = 8 };

// Rows with no department still have to go somewhere the user can find them.
const char *kUnassignedDept = "Unassigned";

QString normalisedDept(const QVariant &raw)
{
    const QString name = raw.toString().trimmed();
    return name.isEmpty() ? QString::fromLatin1(kUnassignedDept) : name;
}
}

QVariantList ExcelHandler::departmentStockSummary() const
{
    // Keyed by the folded name so "electronics" and "Electronics" are one
    // department; the first spelling seen is what gets displayed.
    struct Agg { QString display; int parts = 0; double qty = 0; double value = 0; };
    QMap<QString, Agg> byDept;          // QMap keeps the key order stable

    const int rows = m_model ? m_model->rowCount() : 0;
    double totalQty = 0;
    for (int r = 1; r < rows; ++r) {    // row 0 holds the headers
        const QString partName = m_model->getData(r, ColPartName).toString().trimmed();
        const QString partNo = m_model->getData(r, ColPartNo).toString().trimmed();
        if (partName.isEmpty() && partNo.isEmpty()) continue;   // blank filler row

        const double qty = m_model->getData(r, ColQty).toDouble();
        const double price = m_model->getData(r, ColUnitPrice).toDouble();

        const QString dept = normalisedDept(m_model->getData(r, ColDepartment));
        Agg &a = byDept[dept.toCaseFolded()];
        if (a.display.isEmpty()) a.display = dept;
        a.parts += 1;
        a.qty += qty;
        a.value += qty * price;
        totalQty += qty;
    }

    // Colour slot follows the department's position in the sorted name list, so
    // it does not move when quantities change or a department empties out.
    const QStringList keys = byDept.keys();
    QVariantList out;
    for (auto it = byDept.constBegin(); it != byDept.constEnd(); ++it) {
        QVariantMap m;
        m["department"] = it.value().display;
        m["parts"] = it.value().parts;
        m["qty"] = it.value().qty;
        m["value"] = it.value().value;
        m["share"] = totalQty > 0 ? (it.value().qty / totalQty) : 0.0;
        m["colorIndex"] = keys.indexOf(it.key());
        out.append(m);
    }

    // Biggest holding first - that is the order the reader wants to scan.
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap ma = a.toMap(), mb = b.toMap();
        if (ma["qty"].toDouble() != mb["qty"].toDouble())
            return ma["qty"].toDouble() > mb["qty"].toDouble();
        return ma["department"].toString().localeAwareCompare(mb["department"].toString()) < 0;
    });
    return out;
}

QVariantList ExcelHandler::stockRowsForDepartment(const QString &department) const
{
    QVariantList out;
    const int rows = m_model ? m_model->rowCount() : 0;
    const QString wanted = department.trimmed();

    for (int r = 1; r < rows; ++r) {
        if (!wanted.isEmpty() &&
            normalisedDept(m_model->getData(r, ColDepartment)).compare(
                wanted, Qt::CaseInsensitive) != 0) {
            continue;
        }
        out.append(r);
    }
    return out;
}

QVariantMap ExcelHandler::stockTotals() const
{
    QVariantMap out;
    int parts = 0;
    double units = 0, value = 0;
    QSet<QString> depts;

    const int rows = m_model ? m_model->rowCount() : 0;
    for (int r = 1; r < rows; ++r) {
        const QString partName = m_model->getData(r, ColPartName).toString().trimmed();
        const QString partNo = m_model->getData(r, ColPartNo).toString().trimmed();
        if (partName.isEmpty() && partNo.isEmpty()) continue;

        const double qty = m_model->getData(r, ColQty).toDouble();
        parts += 1;
        units += qty;
        value += qty * m_model->getData(r, ColUnitPrice).toDouble();
        depts.insert(normalisedDept(m_model->getData(r, ColDepartment)).toCaseFolded());
    }

    out["parts"] = parts;
    out["units"] = units;
    out["value"] = value;
    out["departments"] = depts.size();
    return out;
}

void ExcelHandler::refreshFromDatabase()
{
    if (!m_db || !m_db->isConnected()) return;

    // Everything below reflects the database as of now, so this is the point the
    // "have others changed anything?" mark belongs at.
    m_db->syncVersionMark();

    // Polling only pays off against a shared server.
    if (m_db->isServerBackend()) {
        if (!m_liveSyncTimer.isActive()) m_liveSyncTimer.start();
    } else {
        m_liveSyncTimer.stop();
    }

    m_vendors->load();
    m_items->load();
    m_orders->load();
    m_movements->load();
    m_issues->load();
    m_grn->load();
    m_challans->load();
    m_requests->load();
    m_stock->loadFromDb();

    emit vendorListChanged();
    emit itemMasterListChanged();
    emit deliveryChallanListChanged();
    emit purchaseRequestListChanged();
    emit pendingPOCountChanged();
    emit lowStockCountChanged();
}
