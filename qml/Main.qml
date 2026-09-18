import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Basic 6.3 as BasicControls
import ExcelHandler 1.0
import Enstein

ApplicationWindow {
    id: root
    visible: true
    width: 1600
    height: 900
    font.pixelSize: Theme.fontSizeBody
    // A quiet neutral canvas separates operational panels from the window and
    // gives every screen the same professional visual baseline.
    background: Rectangle { color: Theme.workspace }

    property int rows: 0
    property int columns: 0
    property int dataRows: Math.max(0, rows - 1)
    property int selectedRow: -1
    property int contextRow: -1
    property string fileType: "stock"
    property int tableRefreshToken: 0
    property bool dbSetupBusy: false
    // ---- Stock overview / department segregation ----
    // Empty means "all departments"; anything else filters the grid and the chart.
    property string selectedDepartment: ""
    property var deptSummary: []
    property var stockTotalsData: ({})
    property var visibleStockRows: []
    property bool overviewExpanded: true

    // Categorical slots, in fixed order, validated for colour-vision separation
    // against a light surface. Departments take a slot by name (see
    // departmentStockSummary), never by rank, so a colour never migrates between
    // departments when quantities change. Past the sixth, holdings fold into a
    // neutral "Other" slice rather than inventing a new hue.
    readonly property var seriesColors: ["#2a78d6", "#eb6834", "#1baf7a",
                                         "#eda100", "#e87ba4", "#008300"]
    readonly property color seriesOther: "#9aa0a6"
    readonly property int maxColoredSlices: 6

    function departmentColor(colorIndex) {
        if (colorIndex < 0) return seriesOther
        return seriesColors[colorIndex % seriesColors.length]
    }

    function refreshStockOverview() {
        deptSummary = Backend.departmentStockSummary()
        stockTotalsData = Backend.stockTotals()
        visibleStockRows = Backend.stockRowsForDepartment(selectedDepartment)
    }

    // The slices actually drawn: the biggest holdings keep their own colour and
    // the tail becomes one labelled "Other", so the ring never carries more
    // hues than a reader can tell apart.
    function donutSlices() {
        var out = []
        var otherQty = 0
        var otherCount = 0
        for (var i = 0; i < deptSummary.length; i++) {
            var d = deptSummary[i]
            if (d.qty <= 0) continue
            if (out.length < maxColoredSlices) {
                out.push({ label: d.department, qty: d.qty,
                           color: departmentColor(d.colorIndex), isOther: false })
            } else {
                otherQty += d.qty
                otherCount++
            }
        }
        if (otherQty > 0) {
            out.push({ label: "Other (" + otherCount + ")", qty: otherQty,
                       color: seriesOther, isOther: true })
        }
        return out
    }

    property var tableBaseWidths: [350, 190, 130, 200, 180, 200, 260, 190, 140]
    property var tableMinWidths: [140, 90, 70, 90, 90, 90, 110, 90, 80]

    onRowsChanged: refreshStockOverview()
    onTableRefreshTokenChanged: refreshStockOverview()
    onSelectedDepartmentChanged: refreshStockOverview()

    function columnWidth(colIndex) {
        var available = Math.max(800, tableHeader.width - 40)
        var baseTotal = 0
        for (var i = 0; i < tableBaseWidths.length; i++) baseTotal += tableBaseWidths[i]
        var scale = available / baseTotal
        var width = Math.round(tableBaseWidths[colIndex] * scale)
        return Math.max(tableMinWidths[colIndex], width)
    }

    title: {
        var fileName = "Untitled"
        if (Backend.currentFile !== "") {
            var parts = Backend.currentFile.split("/")
            fileName = parts[parts.length - 1]
        }
        return "Enstein Stock Manager - " + fileName +
               (Backend.hasUnsavedChanges ? " *" : "") +
               (loginDialog.isAuthenticated ? " [Logged In]" : " [Read-Only]")
    }

    // Dialogs live in their own files and cannot see this window's id, so the
    // size they need to fit inside is published through the App singleton.
    Binding { target: App; property: "windowWidth";  value: root.width }
    Binding { target: App; property: "windowHeight"; value: root.height }

    // Everything the backend announces, in one place. A screen that needs to
    // react to a change listens here rather than polling for it.
    Connections {
        target: Backend


        function onErrorOccurred(error) {
            errorDialog.errorText = error
            errorDialog.open()
        }

        function onFileLoaded(fileName) {
            statusLabel.text = "Loaded: " + fileName
            root.tableRefreshToken++
            statusTimer.restart()
        }

        function onFileSaved(fileName) {
            statusLabel.text = "Saved: " + fileName
            statusTimer.restart()
        }

        function onFileMerged(fileName, rowsAdded, rowsUpdated) {
            statusLabel.text = "Merged - Added: " + rowsAdded + ", Updated: " + rowsUpdated
            root.tableRefreshToken++
            statusTimer.restart()
        }

        function onSearchResultFound(row) {
            root.selectedRow = row
        }

        // Another machine changed the shared data; it has already been reloaded.
        function onSharedDataChanged() {
            root.rows = Backend.model.rowCount()
            root.columns = Backend.model.columnCount()
            root.tableRefreshToken++
            refreshStockOverview()
            refreshPOList()
            refreshDCList()
            refreshPRList()
            statusLabel.text = "Updated \u2014 someone else changed the shared data"
            statusTimer.restart()
        }

        function onPurchaseOrderCreated(poNo) {
            statusLabel.text = "PO Created: " + poNo
            statusTimer.restart()
        }

        function onDeliveryChallanCreated(dcNo) {
            statusLabel.text = "Delivery Challan Created: " + dcNo
            statusTimer.restart()
        }

        function onDeliveryChallanListChanged() {
            if (dcDialog.visible) refreshDCList()
        }

        function onPurchaseRequestCreated(prNo) {
            statusLabel.text = "Purchase Request raised: " + prNo
            statusTimer.restart()
        }

        function onPurchaseRequestListChanged() {
            if (prDialog.visible) refreshPRList()
        }

        function onGoodsReceived(grnNo, poNo) {
            statusLabel.text = "GRN " + grnNo + " received for " + poNo
            rows = Backend.model.rowCount()
            root.tableRefreshToken++
            refreshPOList()
            refreshMovements()
            statusTimer.restart()
        }

        function onStockIssued(issueNo, partName, qty) {
            statusLabel.text = "Issued: " + partName + " x" + qty + " (" + issueNo + ")"
            rows = Backend.model.rowCount()
            root.tableRefreshToken++
            refreshMovements()
            refreshIssuePartDropdown()
            statusTimer.restart()
        }

        function onServerProvisionProgress(message) {
            dbStatusLabel.text = message
        }

        function onServerProvisionFinished(success, result) {
            root.dbSetupBusy = false
            if (success) {
                // Show the admin exactly what to type into every other
                // computer's connection dialog, and switch this machine's own
                // form over to the freshly provisioned PostgreSQL server.
                dbDriverCombo.currentIndex = 1
                dbHostField.text = result.host !== undefined ? result.host : ""
                dbPortField.text = (result.port !== undefined ? result.port : 5432).toString()
                dbNameField.text = result.name !== undefined ? result.name : "stockmanager"
                dbUserField.text = result.user !== undefined ? result.user : ""
                dbPassField.text = result.password !== undefined ? result.password : ""
                serverInfoBox.visible = true
                dbStatusLabel.text = Backend.databaseStatus()
                statusLabel.text = "This computer is now the shared database server"
            } else {
                dbStatusLabel.text = "Server setup failed: " +
                    (result.message !== undefined ? result.message : "unknown error")
            }
        }
    }

    /* ================= COMMON DIALOGS ================= */

    ErrorDialog { id: errorDialog }

    /* ================= LOGIN DIALOG ================= */

    LoginDialog { id: loginDialog }

    /* ================= FILE ACTIONS ================= */

    function openExcelFile() {
        var selectedFile = Backend.browseOpenFile("Open Excel File", "Excel files (*.xlsx *.xls)")
        if (selectedFile === "") return

        var appended = false
        if (Backend.currentFile !== "" || Backend.permanentFile !== "") {
            if (root.fileType === "stock") {
                appended = Backend.appendStockFile(selectedFile)
                if (appended) {
                    rows = Backend.model.rowCount()
                    columns = Backend.model.columnCount()
                    selectedRow = -1
                    statusLabel.text = "Appended: " + selectedFile
                    statusTimer.restart()
                }
            }
        }

        if (!appended) {
            if (Backend.loadExcel(selectedFile)) {
                rows = Backend.model.rowCount()
                columns = Backend.model.columnCount()
                selectedRow = -1
                root.fileType = Backend.getFileType()
            }
        }
    }

    function saveAsExcelFile() {
        var selectedFile = Backend.browseSaveFile("Save Excel File", "Excel files (*.xlsx)")
        if (selectedFile === "") return
        Backend.saveExcel(selectedFile)
    }

    // Imports the user's stock xlsx into the shared database (login required).
    function importStockFileAction() {
        var selectedFile = Backend.browseOpenFile("Import Stock File", "Excel files (*.xlsx *.xls)")
        if (selectedFile === "") return
        if (Backend.importStockFile(selectedFile)) {
            rows = Backend.model.rowCount()
            columns = Backend.model.columnCount()
            root.fileType = Backend.getFileType()
            statusLabel.text = "Stock file imported into the database"
            statusTimer.restart()
        }
    }

    /* ================= NEW STOCK FILE DIALOG ================= */

    Dialog {
        id: newStockFileDialog
        title: "Create New Stock File"
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        anchors.centerIn: parent

        ColumnLayout {
            spacing: 10
            Label { text: "Stock File (with Supply Chain columns)"; font.bold: true; font.pixelSize: 14; color: Theme.success }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.success }
            Label { text: "Number of Rows:"; font.bold: true }
            SpinBox { id: stockRowsSpin; from: 1; to: 1000; value: 15; editable: true }
            Label {
                text: "Columns: Part Name | Part No | Stock | Department | Prepared | Approved | Vendor | Date | Unit Price"
                font.pixelSize: 10; color: Theme.textSecondary; wrapMode: Text.WordWrap
            }
        }

        onAccepted: {
            Backend.createStockFile(stockRowsSpin.value)
            rows = Backend.model.rowCount()
            columns = Backend.model.columnCount()
            selectedRow = -1
            root.fileType = "stock"
            statusLabel.text = "Created new Stock file: " + rows + " rows (9 columns)"
        }
    }

    /* ================= SEARCH DIALOG ================= */

    StockSearchDialog {
        id: searchDialog
        onRowPicked: (row) => {
            root.selectedRow = row
            var pos = row - 1
            if (pos >= 0) tableListView.positionViewAtIndex(pos, ListView.Center)
        }
    }

    /* ================= ADD ITEM DIALOG ================= */

    Dialog {
        id: addItemDialog
        title: "Add Stock Item"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        width: 450

        ColumnLayout {
            spacing: 10; width: parent.width

            Label { text: "Add New Stock Item"; font.bold: true; font.pixelSize: 14 }
            Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

            Label { text: "Part Name:" }
            TextField { id: partNameField; Layout.fillWidth: true; placeholderText: "e.g., Microcontroller"; selectByMouse: true }

            Label { text: "Department:" }
            ComboBox { id: categoryField; Layout.fillWidth: true; editable: true; model: ["Electronics", "Mechanical","Electrical","SCM", "Software", "Other"] }

            Label { text: "Stock Quantity:" }
            SpinBox { id: quantityField; Layout.fillWidth: true; from: 1; to: 10000; value: 1; editable: true }

            Label { text: "Unit Price:" }
            TextField {
                id: unitPriceField
                Layout.fillWidth: true
                placeholderText: "0.00"
                text: "0.00"
                validator: DoubleValidator { bottom: 0; decimals: 2 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
            }

            Label { text: "If part exists, stock will be added to current quantity"; font.pixelSize: 10; color: Theme.textSecondary; wrapMode: Text.WordWrap }
        }

        onAccepted: {
            if (partNameField.text.trim() === "") { errorDialog.errorText = "Part name is required!"; errorDialog.open(); return }
            Backend.addNewItem(partNameField.text.trim(), categoryField.currentText, quantityField.value, Format.amount(unitPriceField.text))
            rows = Backend.model.rowCount()
            columns = Backend.model.columnCount()
            statusLabel.text = "Added: " + partNameField.text; statusTimer.restart()
            partNameField.text = ""; quantityField.value = 1; unitPriceField.text = "0.00"
        }

        onOpened: { partNameField.focus = true }
    }

    /* ================= VENDOR MANAGEMENT DIALOG ================= */

    VendorMasterDialog {
        id: vendorDialog
        onDropdownsStale: refreshVendorDropdowns()
        onVendorActivated: (vendor) => vendorDetailsDialog.showVendor(vendor)
    }

    /* ================= VENDOR DETAILS DIALOG ================= */

    VendorDetailsDialog {
        id: vendorDetailsDialog
        onVendorChanged: vendorDialog.refresh()
        onDropdownsStale: refreshVendorDropdowns()
    }


    function refreshVendorDropdowns() {
        var vendorNames = Backend.getVendorNames()
        itemMasterDialog.refreshVendorOptions(vendorNames)
        poVendorField.model = vendorNames
        itemDetailsDialog.refreshVendorOptions(vendorNames)
        prVendorField.model = vendorNames
    }



    function applyPoPartDetails(partName) {
        var key = (partName || "").toString().trim()
        var partNo = poPartLookup[key]
        if (partNo !== undefined && partNo !== null) {
            poPartNoField.text = partNo.toString()
        } else if (key === "") {
            poPartNoField.text = ""
        }
        var details = poPartDetailsLookup[key]
        if (details !== undefined && details !== null) {
            poUnitPriceField.text = Format.fixed(details.unitPrice)
            poDepartmentField.text = details.department || ""
            poDepartmentAutoFilled = true
            // Auto-select the vendor that belongs to this item (from the
            // Item Master); the user can still change it before adding.
            if ((details.vendor || "").toString().trim() !== "") {
                poVendorField.editText = details.vendor
            }
        } else if (poDepartmentAutoFilled) {
            // Part is not in the Item Master. Only clear a department we filled
            // in ourselves, never one the user typed for a one-off item.
            poDepartmentField.text = ""
        }
    }

    // ---- Printable purchase order ----

    property string poPreviewPoNo: ""
    property var poPreviewPages: []
    property string poPreviewPdfPath: ""

    function openPoPdfPreview(poNo) {
        poPreviewPoNo = poNo
        poPreviewCommentsField.text = ""
        refreshPoPdfPreview()
        if (poPreviewPages.length > 0)
            poPdfPreviewDialog.open()
    }

    function refreshPoPdfPreview() {
        var result = Backend.generatePOPreview(poPreviewPoNo,
                                                    poPreviewCommentsField.text)
        if (!result || !result.pages || result.pages.length === 0) {
            poPreviewPages = []
            poPreviewPdfPath = ""
            statusLabel.text = "Could not render the purchase order PDF"
            statusTimer.restart()
            return
        }
        poPreviewPages = result.pages
        poPreviewPdfPath = result.pdfPath
    }

    function sendPoPdf() {
        var saved = Backend.savePOPdf(poPreviewPoNo, "",
                                           poPreviewCommentsField.text)
        if (saved === "") return
        poLastSavedPdf = saved
        statusLabel.text = "Purchase order saved to " + saved
        statusTimer.restart()
        poPdfPreviewDialog.close()
        poSavedDialog.open()
    }

    property string poLastSavedPdf: ""

    // ---- Searchable pickers for the PO line entry ----
    //
    // The plain dropdowns stop being usable once the Item Master or vendor list
    // grows past a screenful, so both fields also get a search dialog that
    // filters across every field, not just the name.

    ListModel { id: poPartPickerModel }
    ListModel { id: poVendorPickerModel }

    function refreshPoPartPicker(filter) {
        poPartPickerModel.clear()
        var f = (filter || "").toString().trim().toLowerCase()
        var items = Backend.getItemMasterList()
        for (var i = 0; i < items.length; i++) {
            var it = items[i]
            var partName = (it.partName || "").toString()
            if (partName.trim() === "") continue
            var dept = (it.department || it.category || "").toString()
            var partNo = (it.partNo || "").toString()
            var vendor = (it.vendor || "").toString()
            if (f !== "" &&
                (partName + " " + partNo + " " + dept + " " + vendor).toLowerCase().indexOf(f) === -1)
                continue
            poPartPickerModel.append({
                partName: partName, partNo: partNo, department: dept, vendor: vendor,
                unitPrice: it.unitPrice || 0,
                requiredQty: it.requiredQty || it.stockQty || 0
            })
        }
    }

    function refreshPoVendorPicker(filter) {
        poVendorPickerModel.clear()
        var f = (filter || "").toString().trim().toLowerCase()
        var vendors = Backend.getVendorList()
        for (var i = 0; i < vendors.length; i++) {
            var v = vendors[i]
            var name = (v.vendorName || "").toString()
            if (name.trim() === "") continue
            var phone = (v.phone || "").toString()
            var email = (v.email || "").toString()
            var contact = (v.contactPerson || "").toString()
            var gstin = (v.gstin || "").toString()
            if (f !== "" &&
                (name + " " + phone + " " + email + " " + contact + " " + gstin).toLowerCase().indexOf(f) === -1)
                continue
            poVendorPickerModel.append({
                name: name, phone: phone, email: email,
                contactPerson: contact, gstin: gstin
            })
        }
    }

    function openPoPartPicker() {
        poPartSearchField.text = ""
        refreshPoPartPicker("")
        poPartPickerDialog.open()
        poPartSearchField.forceActiveFocus()
    }

    // The field the vendor picker will fill in - a ComboBox or a plain
    // TextField, whichever asked for it.
    property var vendorPickerTarget: null

    function openVendorPicker(target) {
        vendorPickerTarget = target
        poVendorSearchField.text = ""
        refreshPoVendorPicker("")
        poVendorPickerDialog.open()
        poVendorSearchField.forceActiveFocus()
    }

    function choosePoPart(partName) {
        // Prefer selecting the real dropdown entry so the ComboBox stays in
        // sync; fall back to free text for anything not in the Item Master.
        var idx = poPartNameField.find(partName)
        if (idx >= 0) poPartNameField.currentIndex = idx
        else poPartNameField.editText = partName
        applyPoPartDetails(partName)
        poPartPickerDialog.close()
    }

    function chooseVendorFromPicker(name) {
        var target = vendorPickerTarget
        if (target) {
            if (target.find !== undefined) {
                // A ComboBox: prefer selecting the real entry so it stays in
                // sync, and fall back to free text for anything not listed.
                var idx = target.find(name)
                if (idx >= 0) target.currentIndex = idx
                else target.editText = name
            } else {
                target.text = name
            }
        }
        poVendorPickerDialog.close()
    }

    Connections {
        target: Backend
        function onItemMasterListChanged() {
            itemMasterDialog.refresh()
            refreshPartNameDropdown()
            if (poDialog.visible) {
                applyPoPartDetails(poPartNameField.currentText)
            }
        }
        function onVendorListChanged() {
            vendorDialog.refresh()
            refreshVendorDropdowns()
        }
    }



    /* =================  ITEM MASTER DIALOG =================== */

    ItemMasterDialog {
        id: itemMasterDialog
        onDropdownsStale: refreshVendorDropdowns()
        onPartNamesStale: (items) => refreshPartNameDropdown(items)
        onItemActivated: (item) => itemDetailsDialog.showItem(item)
        onVendorPickerRequested: (field) => openVendorPicker(field)
    }

    /* ================= ITEM DETAILS DIALOG ================= */

    ItemDetailsDialog {
        id: itemDetailsDialog
        onItemChanged: itemMasterDialog.refresh()
        onVendorPickerRequested: (field) => openVendorPicker(field)
    }


    property var poPartLookup: ({})
    property var poPartDetailsLookup: ({})
    // False once the user types a department by hand, which stops the Item
    // Master autofill from clearing it out from under them.
    property bool poDepartmentAutoFilled: true
    property var selectedPoPartDetails: ({})
    property var selectedPODetails: ({})
    property var selectedPOItems: []

    function refreshPartNameDropdown(items) {
        var sourceItems = items
        if (sourceItems === undefined || sourceItems === null) {
            sourceItems = Backend.getItemMasterList()
        }

        var partNames = []
        var lookup = ({})
        var detailsLookup = ({})

        for (var i = 0; i < sourceItems.length; i++) {
            var item = sourceItems[i]
            var partName = (item.partName || "").toString().trim()
            if (partName === "") continue

            if (lookup[partName] === undefined) {
                partNames.push(partName)
            }
            lookup[partName] = item.partNo || ""
            detailsLookup[partName] = {
                partName: partName,
                partNo: item.partNo || "",
                department: item.department || item.category || "",
                requiredQty: item.requiredQty || item.stockQty || 0,
                unitPrice: item.unitPrice || 0,
                vendor: item.vendor || "",
                // The challan has one HSN/SAC column, so it gets the code that
                // matches how the item is classified.
                hsnCode: item.taxCode || item.hsnCode || "",
                unit: item.unit || ""
            }
        }

        poPartLookup = lookup
        poPartDetailsLookup = detailsLookup
        poPartNameField.model = partNames
        dcItemNameField.model = partNames
    }

    ListModel { id: issueSearchModel }

    function refreshIssuePartDropdown(filterText) {
        issueSearchModel.clear()
        var query = (filterText || "").toString().trim().toLowerCase()
        var totalRows = Backend.model.rowCount()

        for (var i = 1; i < totalRows; i++) {
            var partName = (Backend.model.getData(i, 0) || "").toString().trim()
            if (partName === "") continue

            var stock = parseInt(Backend.model.getData(i, 2)) || 0
            if (stock <= 0) continue

            if (query !== "" && partName.toLowerCase().indexOf(query) === -1) continue
            issueSearchModel.append({ name: partName, stock: stock })
        }

        issuePartField.model = issueSearchModel
    }

    // Opens the shared calendar for a date field. seedField supplies the month
    // to land on when the target is still empty, so picking the end of a period
    // starts from its beginning rather than from today.
    function openDatePicker(field, purpose, seedField) {
        expectedDateDialog.targetField = field
        expectedDateDialog.purpose = purpose
        expectedDateDialog.seedText = (field.text === "" && seedField) ? seedField.text : field.text
        expectedDateDialog.open()
    }

    function showPoPartDetails(partName) {
        var key = (partName || "").toString().trim()
        if (key === "") return
        var details = poPartDetailsLookup[key]
        if (details === undefined || details === null) {
            statusLabel.text = "No Item Master details for: " + key
            statusTimer.restart()
            return
        }
        selectedPoPartDetails = details
        poPartDetailsDialog.open()
    }

    property string pendingPOForApproval: ""
    property string editingPONumber: ""

    /* ================= PURCHASE ORDER DIALOG ================= */

    // Items being collected for the next purchase order (the "cart").
    ListModel { id: poCartModel }
    property double poCartTotal: 0

    function recalcPoCartTotal() {
        var t = 0
        for (var i = 0; i < poCartModel.count; i++) t += poCartModel.get(i).lineTotal
        poCartTotal = t
    }

    function clearPoLineFields() {
        poPartNameField.currentIndex = -1
        poPartNameField.editText = ""
        poPartNoField.text = ""
        poDepartmentField.text = ""
        poDepartmentAutoFilled = true
        poVendorField.currentIndex = -1
        poVendorField.editText = ""
        poQtyField.value = 1
        poUnitPriceField.text = "0.00"
    }

    function addPoItemToCart() {
        var partName = (poPartNameField.editText || poPartNameField.currentText || "").toString().trim()
        var vendor = (poVendorField.editText || poVendorField.currentText || "").toString().trim()
        if (partName === "") { statusLabel.text = "Select a part first"; statusTimer.restart(); return }
        if (vendor === "") { statusLabel.text = "Select a vendor for " + partName; statusTimer.restart(); return }

        var qty = poQtyField.value
        var price = Format.amount(poUnitPriceField.text)
        poCartModel.append({
            partName: partName,
            partNo: poPartNoField.text,
            vendor: vendor,
            department: poDepartmentField.text,
            qty: qty,
            unitPrice: price,
            lineTotal: qty * price
        })
        recalcPoCartTotal()
        clearPoLineFields()
    }

    Dialog {
        id: poDialog
        title: "Purchase Order Management"
        modal: true
        anchors.centerIn: parent
        width: parent ? Math.min(1140, parent.width - 60) : 1140
        height: parent ? Math.min(840, parent.height - 60) : 840

        ColumnLayout {
            anchors.fill: parent; spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Purchase Orders"; font.bold: true; font.pixelSize: 16; color: Theme.textPrimary }
                Item { Layout.fillWidth: true }
                Label {
                    text: "Raising this order for " + root.poFromRequest
                    visible: root.poFromRequest !== ""
                    font.pixelSize: 12; font.bold: true; color: Theme.warning
                }
            }

            // Create PO Form - add one or more items to the cart, then create.
            // The card takes its height from the grid so the editors always get
            // their full height instead of being squeezed into a fixed box.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: poEntryGrid.implicitHeight + 2 * poEntryGrid.anchors.margins
                color: Theme.infoWash; border.color: Theme.info; radius: 5

                GridLayout {
                    id: poEntryGrid
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 14
                    columns: 4; rowSpacing: 12; columnSpacing: 16

                    Label {
                        text: "ORDER LINE DETAILS"
                        Layout.columnSpan: 4; Layout.fillWidth: true
                        font.bold: true; font.pixelSize: 11; color: Theme.textSecondary
                    }

                    // The two caption columns keep a fixed width so both field
                    // columns start on the same edge.
                    Label {
                        text: "Next PO:"; font.bold: true
                        Layout.minimumWidth: 92; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    Label { text: Backend.getNextPONumber(); color: Theme.info; font.bold: true; Layout.fillWidth: true }
                    Label {
                        text: "Date:"
                        Layout.minimumWidth: 92; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    Label { text: Qt.formatDate(new Date(), "yyyy-MM-dd"); Layout.fillWidth: true }

                    Label {
                        text: "Part Name*:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        ComboBox {
                            id: poPartNameField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            editable: true
                            model: []
                            onCurrentTextChanged: applyPoPartDetails(currentText)
                            onActivated: function(index) {
                                if (index >= 0) showPoPartDetails(currentText)
                            }
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search the Item Master"
                            onClicked: openPoPartPicker()
                        }
                        Button {
                            text: "i"
                            font.bold: true
                            Layout.preferredWidth: 34
                            ToolTip.visible: hovered
                            ToolTip.text: "Show selected part details"
                            onClicked: showPoPartDetails(poPartNameField.currentText)
                        }
                    }
                    Label {
                        text: "Vendor*:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        ComboBox {
                            id: poVendorField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            editable: true
                            model: Backend.getVendorNames()
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search vendors"
                            onClicked: openVendorPicker(poVendorField)
                        }
                    }

                    Label {
                        text: "Part No:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField { id: poPartNoField; Layout.fillWidth: true; placeholderText: "Part number"; selectByMouse: true }
                    Label {
                        text: "Department:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: poDepartmentField
                        Layout.fillWidth: true
                        selectByMouse: true
                        placeholderText: "Auto-filled from Item Master, or type one"
                        // textEdited fires only on user input, so the autofill
                        // above does not count as the user claiming the field.
                        onTextEdited: poDepartmentAutoFilled = false
                    }

                    Label {
                        text: "Qty*:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    SpinBox { id: poQtyField; from: 1; to: 100000; value: 1; editable: true; Layout.fillWidth: true }
                    Label {
                        text: "Unit Price:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: "\u20B9"; font.bold: true; color: Theme.textPrimary }
                        TextField {
                            id: poUnitPriceField
                            Layout.fillWidth: true
                            // Wide enough to show a crore-scale rate in full.
                            Layout.minimumWidth: 170
                            placeholderText: "0.00"
                            text: "0.00"
                            horizontalAlignment: TextInput.AlignRight
                            validator: DoubleValidator { bottom: 0; decimals: 2 }
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            selectByMouse: true
                        }
                    }

                    Label {
                        text: "Line Total:"; font.bold: true
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    // Spans both field columns so even a crore total fits.
                    Label {
                        text: Format.rupees(poQtyField.value * Format.amount(poUnitPriceField.text))
                        color: Theme.success
                        font.bold: true
                        font.pixelSize: 15
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "+ Add Item"
                        highlighted: true
                        Layout.minimumWidth: 120
                        Layout.alignment: Qt.AlignRight
                        onClicked: addPoItemToCart()
                    }
                }
            }

            // Cart: items that will go into this PO. It grows with the item
            // count up to four rows and then scrolls, so an empty cart does not
            // steal vertical room from the PO list below.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: poCartModel.count === 0
                                        ? 64
                                        : Math.min(196, 48 + poCartModel.count * 40)
                border.color: Theme.info; radius: 5; color: "#fbfdff"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 6

                    // Column captions, matching the delegate widths below, so
                    // the qty/rate/amount columns read as a table.
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6; Layout.rightMargin: 6
                        spacing: 10
                        visible: poCartModel.count > 0
                        Label { text: "Item"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.fillWidth: true }
                        Label { text: "Vendor"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 190 }
                        Label { text: "Qty"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                        Label { text: "Rate"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 170; horizontalAlignment: Text.AlignRight }
                        Label { text: "Amount"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 180; horizontalAlignment: Text.AlignRight }
                        Item { Layout.preferredWidth: 30 }
                    }

                    ListView {
                        id: poCartView
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; spacing: 6
                        model: poCartModel
                        delegate: Rectangle {
                            width: poCartView.width; height: 34
                            color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 6
                                anchors.topMargin: 3; anchors.bottomMargin: 3
                                spacing: 10
                                Label {
                                    text: (index + 1) + ". " + model.partName
                                    font.bold: true; font.pixelSize: 11
                                    Layout.fillWidth: true; Layout.minimumWidth: 120
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: model.vendor
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.preferredWidth: 190; elide: Text.ElideRight
                                }
                                Label {
                                    text: model.qty
                                    font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight
                                }
                                Label {
                                    text: Format.rupees(model.unitPrice)
                                    font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.preferredWidth: 170; horizontalAlignment: Text.AlignRight
                                }
                                Label {
                                    text: Format.rupees(model.lineTotal)
                                    font.pixelSize: 12; font.bold: true; color: Theme.success
                                    Layout.preferredWidth: 180; horizontalAlignment: Text.AlignRight
                                }
                                Button {
                                    Layout.preferredWidth: 30; Layout.preferredHeight: 24
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Remove this item"
                                    onClicked: { poCartModel.remove(index); recalcPoCartTotal() }
                                    contentItem: Text { text: "X"; color: Theme.danger; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No items added - use \"+ Add Item\" above (a PO can hold several items)"
                    visible: poCartModel.count === 0; color: Theme.textMuted; font.pixelSize: 11
                }
            }

            // The period the goods are needed in, plus who prepared the order
            RowLayout {
                Layout.fillWidth: true; spacing: 10

                Label { text: "Needed from:"; font.bold: true }
                TextField {
                    id: poExpectedField
                    Layout.preferredWidth: 120
                    placeholderText: "YYYY-MM-DD"
                    readOnly: true
                    selectByMouse: true
                }
                Button {
                    text: "\u{1F4C5}"
                    Layout.preferredWidth: 38
                    ToolTip.visible: hovered
                    ToolTip.text: "Pick the start of the period"
                    onClicked: openDatePicker(poExpectedField, "period start")
                }

                Label { text: "to:"; font.bold: true }
                TextField {
                    id: poExpectedEndField
                    Layout.preferredWidth: 120
                    placeholderText: "YYYY-MM-DD"
                    readOnly: true
                    selectByMouse: true
                }
                Button {
                    text: "\u{1F4C5}"
                    Layout.preferredWidth: 38
                    ToolTip.visible: hovered
                    ToolTip.text: "Pick the end of the period"
                    onClicked: openDatePicker(poExpectedEndField, "period end", poExpectedField)
                }
                Button {
                    text: "\u2715"
                    Layout.preferredWidth: 32
                    enabled: poExpectedField.text !== "" || poExpectedEndField.text !== ""
                    ToolTip.visible: hovered
                    ToolTip.text: "Clear the period"
                    onClicked: { poExpectedField.text = ""; poExpectedEndField.text = "" }
                }

                // Reads back the period so the requirement is unmistakable, and
                // says so plainly when the dates are the wrong way round.
                Label {
                    property int days: Format.periodDays(poExpectedField.text, poExpectedEndField.text)
                    property bool reversed: poExpectedField.text !== "" &&
                                            poExpectedEndField.text !== "" &&
                                            poExpectedEndField.text < poExpectedField.text
                    text: reversed
                          ? "End date is before the start date"
                          : (days > 0 ? days + (days === 1 ? " day" : " days") : "")
                    color: reversed ? Theme.danger : Theme.textSecondary
                    font.pixelSize: 11
                    font.bold: reversed
                }

                Item { Layout.fillWidth: true }

                Label { text: "Prepared By*:" }
                TextField {
                    id: poPreparedByField
                    Layout.preferredWidth: 180
                    placeholderText: "Enter preparer name"
                    text: Backend.currentUser
                    selectByMouse: true
                }
            }

            // Cart total + create
            RowLayout {
                Layout.fillWidth: true; spacing: 12

                Item { Layout.fillWidth: true }

                // Cart total gets its own boxed area so it stays legible even
                // at crore values instead of squeezing the fields beside it.
                Rectangle {
                    Layout.preferredWidth: 260
                    Layout.preferredHeight: 38
                    Layout.leftMargin: 6
                    color: "#eafaf1"; border.color: Theme.success; radius: 4
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                        Label { text: "Total"; font.bold: true; color: Theme.textPrimary; font.pixelSize: 12 }
                        Label {
                            text: Format.rupees(poCartTotal)
                            font.bold: true; font.pixelSize: 15; color: Theme.success
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Button {
                    text: "Create PO (" + poCartModel.count + (poCartModel.count === 1 ? " item)" : " items)")
                    highlighted: true
                    enabled: poCartModel.count > 0
                    onClicked: {
                        // A backwards period would print nonsense on the order,
                        // so stop before anything is written.
                        if (poExpectedField.text !== "" && poExpectedEndField.text !== "" &&
                            poExpectedEndField.text < poExpectedField.text) {
                            statusLabel.text = "Needed period: the end date cannot be before the start date"
                            statusTimer.restart()
                            return
                        }
                        var items = []
                        for (var i = 0; i < poCartModel.count; i++) {
                            var it = poCartModel.get(i)
                            items.push({
                                partName: it.partName, partNo: it.partNo,
                                vendor: it.vendor, department: it.department,
                                qty: it.qty, unitPrice: it.unitPrice
                            })
                        }
                        var poNo = Backend.createPurchaseOrderItems(
                            items, poExpectedField.text, poExpectedEndField.text,
                            poPreparedByField.text)
                        if (poNo !== "") {
                            poCartModel.clear()
                            recalcPoCartTotal()
                            poExpectedField.text = ""
                            poExpectedEndField.text = ""
                            refreshPOList()
                            // Only now that the order really exists is the
                            // request marked as bought.
                            if (root.poFromRequest !== "") {
                                Backend.linkRequestToPO(root.poFromRequest, poNo)
                                statusLabel.text = poNo + " raised for " + root.poFromRequest
                                statusTimer.restart()
                                root.poFromRequest = ""
                                refreshPRList()
                            }
                            openPoPdfPreview(poNo)
                        }
                    }
                }
            }

            // Status filter + free-text search across the PO list
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 4; spacing: 10

                Label { text: "Filter:"; font.bold: true }
                ComboBox {
                    id: poFilterCombo
                    Layout.preferredWidth: 170
                    model: ["All", "Draft", "Sent", "Partially Received", "Received", "Closed"]
                    onCurrentTextChanged: refreshPOList()
                }

                Label { text: "Search:"; font.bold: true; Layout.leftMargin: 6 }
                TextField {
                    id: poSearchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 240
                    placeholderText: "PO number, vendor, part name, or date (e.g. 2026-08)"
                    selectByMouse: true
                    onTextChanged: applyPoSearch()
                    // Clear the search first; only pass Escape on to the dialog
                    // once there is nothing left to clear.
                    Keys.onEscapePressed: function(event) {
                        if (text !== "") text = ""
                        else event.accepted = false
                    }
                }
                Button {
                    text: "\u2715"
                    Layout.preferredWidth: 34
                    enabled: poSearchField.text !== ""
                    ToolTip.visible: hovered
                    ToolTip.text: "Clear the search"
                    onClicked: poSearchField.text = ""
                }

                Label {
                    text: poSearchField.text.trim() === ""
                          ? poListModel.count + (poListModel.count === 1 ? " PO" : " POs")
                          : poListModel.count + " of " + poRowsCache.length + " match"
                    color: Theme.textSecondary; font.pixelSize: 11
                }
                Label { text: "Pending: " + Backend.pendingPOCount; font.bold: true; color: Theme.warning }
            }

            // PO List
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 5

                ListView {
                    id: poListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: ListModel { id: poListModel }
                    delegate: Rectangle {
                        width: poListView.width - 10; height: 78
                        color: {
                            if (model.status === "Received") return Theme.successWash
                            if (model.status === "Draft") return Theme.warningWash
                            return "#fff"
                        }
                        border.color: Theme.borderSubtle; radius: 4

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 180; spacing: 3
                                RowLayout {
                                    spacing: 8
                                    Label { text: model.poNo; font.bold: true; font.pixelSize: 13; color: Theme.textPrimary }
                                    Label {
                                        text: model.status
                                        font.pixelSize: 11; font.bold: true
                                        color: {
                                            if (model.status === "Draft") return Theme.pending
                                            if (model.status === "Sent") return Theme.info
                                            if (model.status === "Received") return Theme.success
                                            if (model.status === "Partially Received") return Theme.warning
                                            return Theme.textMuted
                                        }
                                    }
                                }
                                Label {
                                    text: model.partName + "  |  Qty: " + model.qty + "  |  Vendor: " + model.vendor
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Label {
                                    text: "Date: " + model.date +
                                          "  |  Needed: " + Format.period(model.expectedDate, model.expectedEndDate) +
                                          "  |  Received: " + model.receivedQty + "/" + model.qty +
                                          "  |  Rec Date: " + (model.receivedDate || "-")
                                    font.pixelSize: 10
                                    color: Theme.textMuted
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                            }

                            // Amounts get a dedicated right-aligned column so a
                            // crore-scale total is never elided by the details.
                            ColumnLayout {
                                Layout.preferredWidth: 200
                                Layout.alignment: Qt.AlignVCenter
                                spacing: 2
                                Label {
                                    text: Format.rupees(model.totalAmount)
                                    font.bold: true; font.pixelSize: 14; color: Theme.success
                                    Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                }
                                Label {
                                    // A single rate only means something on a
                                    // one-line PO; otherwise show the line count.
                                    text: model.unitPrice > 0
                                          ? Format.rupees(model.unitPrice) + " / unit"
                                          : (model.itemCount > 1 ? model.itemCount + " line items" : "")
                                    visible: text !== ""
                                    font.pixelSize: 10; color: Theme.textMuted
                                    Layout.fillWidth: true; horizontalAlignment: Text.AlignRight
                                }
                            }

                            // Status buttons
                            Button {
                                text: "View"
                                onClicked: {
                                    selectedPODetails = {
                                        poNo: model.poNo,
                                        date: model.date,
                                        vendor: model.vendor,
                                        partName: model.partName,
                                        partNo: model.partNo,
                                        department: model.department || "",
                                        qty: model.qty,
                                        unitPrice: model.unitPrice,
                                        totalAmount: model.totalAmount,
                                        expectedDate: model.expectedDate,
                                        expectedEndDate: model.expectedEndDate,
                                        status: model.status,
                                        receivedQty: model.receivedQty,
                                        preparedBy: model.preparedBy || "",
                                        approvedBy: model.approvedBy || "",
                                        receivedBy: model.receivedBy || "",
                                        receivedDate: model.receivedDate || ""
                                    }
                                    selectedPOItems = Backend.getPOItems(model.poNo)
                                    poDetailsDialog.open()
                                }
                                contentItem: Text { text: "View"; color: Theme.textPrimary; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Edit"
                                visible: model.status !== "Received" && model.status !== "Closed" &&
                                         (model.itemCount === undefined || model.itemCount <= 1)
                                onClicked: {
                                    editingPONumber = model.poNo
                                    editPoVendorField.text = model.vendor || ""
                                    editPoPartNameField.text = model.partName || ""
                                    editPoPartNoField.text = model.partNo || ""
                                    editPoDepartmentField.text = model.department || ""
                                    editPoQtyField.value = model.qty || 1
                                    editPoUnitPriceField.text = Format.fixed(model.unitPrice)
                                    editPoExpectedField.text = model.expectedDate || ""
                                    editPoExpectedEndField.text = model.expectedEndDate || ""
                                    editPoPreparedByField.text = model.preparedBy || Backend.currentUser
                                    poEditDialog.open()
                                }
                                contentItem: Text { text: "Edit"; color: "#8e44ad"; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Send"
                                visible: model.status === "Draft"
                                onClicked: {
                                    pendingPOForApproval = model.poNo
                                    approvalDialog.showForApproval(model.poNo)
                                }
                                contentItem: Text { text: "Send"; color: Theme.info; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Receive"
                                visible: model.status === "Sent" || model.status === "Partially Received"
                                onClicked: {
                                    grnPOFieldText = model.poNo
                                    grnLoadItems(model.poNo)
                                    grnRejectedField.value = 0
                                    grnReceivedByField.text = Backend.currentUser
                                    grnDialog.open()
                                }
                                contentItem: Text { text: "Receive"; color: Theme.success; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Close"
                                visible: model.status === "Received"
                                onClicked: { Backend.updatePOStatus(model.poNo, "Closed"); refreshPOList() }
                                contentItem: Text { text: "Close"; color: Theme.textMuted; font.pixelSize: 11 }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: poSearchField.text.trim() === ""
                          ? "No purchase orders"
                          : "No purchase orders match \"" + poSearchField.text.trim() + "\""
                    visible: poListModel.count === 0; color: Theme.textMuted
                }
            }

            Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: poDialog.close() }
        }

        onOpened: {
            poSearchField.text = ""
            refreshPOList()
            refreshVendorDropdowns()
            refreshPartNameDropdown()
        }

        // Closing without ordering abandons the hand-off, so the request stays
        // approved and waiting rather than being marked as bought.
        onClosed: root.poFromRequest = ""
    }

    PoApprovalDialog {
        id: approvalDialog
        poNo: root.pendingPOForApproval
        onOrderChanged: refreshPOList()
    }

    Dialog {
        id: poEditDialog
        title: "Edit Purchase Order"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 560

        ColumnLayout {
            spacing: 10
            width: parent.width

            Label { text: "PO: " + editingPONumber; font.bold: true; color: Theme.textPrimary }

            GridLayout {
                columns: 2
                rowSpacing: 8
                columnSpacing: 10
                Layout.fillWidth: true

                Label { text: "Vendor:" }
                TextField { id: editPoVendorField; Layout.fillWidth: true; selectByMouse: true }

                Label { text: "Part Name:" }
                TextField { id: editPoPartNameField; Layout.fillWidth: true; selectByMouse: true }

                Label { text: "Part No:" }
                TextField { id: editPoPartNoField; Layout.fillWidth: true; selectByMouse: true }

                Label { text: "Department:" }
                TextField { id: editPoDepartmentField; Layout.fillWidth: true; selectByMouse: true }

                Label { text: "Qty:" }
                SpinBox { id: editPoQtyField; from: 1; to: 100000; value: 1; editable: true; Layout.fillWidth: true }

                Label { text: "Unit Price:" }
                TextField {
                    id: editPoUnitPriceField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 170
                    placeholderText: "0.00"
                    text: "0.00"
                    horizontalAlignment: TextInput.AlignRight
                    validator: DoubleValidator { bottom: 0; decimals: 2 }
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    selectByMouse: true
                }

                Label { text: "Needed From:" }
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    TextField {
                        id: editPoExpectedField
                        Layout.fillWidth: true
                        placeholderText: "YYYY-MM-DD"
                        selectByMouse: true
                    }
                    Button {
                        text: "\u{1F4C5}"
                        Layout.preferredWidth: 38
                        ToolTip.visible: hovered
                        ToolTip.text: "Pick the start of the period"
                        onClicked: openDatePicker(editPoExpectedField, "period start")
                    }
                }

                Label { text: "Needed To:" }
                RowLayout {
                    Layout.fillWidth: true; spacing: 6
                    TextField {
                        id: editPoExpectedEndField
                        Layout.fillWidth: true
                        placeholderText: "YYYY-MM-DD (optional)"
                        selectByMouse: true
                    }
                    Button {
                        text: "\u{1F4C5}"
                        Layout.preferredWidth: 38
                        ToolTip.visible: hovered
                        ToolTip.text: "Pick the end of the period"
                        onClicked: openDatePicker(editPoExpectedEndField, "period end", editPoExpectedField)
                    }
                }

                Label { text: "Prepared By:" }
                TextField { id: editPoPreparedByField; Layout.fillWidth: true; selectByMouse: true }
            }

            Label {
                text: "Total: " + Format.rupees(editPoQtyField.value * Format.amount(editPoUnitPriceField.text))
                font.bold: true
                font.pixelSize: 15
                color: Theme.success
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
            }
        }

        onAccepted: {
                if (editPoExpectedField.text !== "" && editPoExpectedEndField.text !== "" &&
                    editPoExpectedEndField.text < editPoExpectedField.text) {
                    statusLabel.text = "Needed period: the end date cannot be before the start date"
                    statusTimer.restart()
                    return
                }
                var payload = {
                    vendor: editPoVendorField.text,
                    partName: editPoPartNameField.text,
                    partNo: editPoPartNoField.text,
                    department: editPoDepartmentField.text,
                    qty: editPoQtyField.value,
                    unitPrice: Format.amount(editPoUnitPriceField.text),
                    expectedDate: editPoExpectedField.text,
                    expectedEndDate: editPoExpectedEndField.text,
                    preparedBy: editPoPreparedByField.text
                }

            if (Backend.updatePurchaseOrder(editingPONumber, payload)) {
                refreshPOList()
            }
        }
    }

    PoDetailsDialog {
        id: poDetailsDialog
        order: root.selectedPODetails
        lineItems: root.selectedPOItems
    }

    PoPdfPreviewDialog {
        id: poPdfPreviewDialog
        poNo: root.poPreviewPoNo
        pages: root.poPreviewPages
        pdfPath: root.poPreviewPdfPath
        onRefreshRequested: refreshPoPdfPreview()
        onSendRequested: sendPoPdf()
    }

    PoSavedDialog {
        id: poSavedDialog
        pdfPath: root.poLastSavedPdf
    }

    CompanyProfileDialog {
        id: companyProfileDialog
    }

    Dialog {
        id: poPartPickerDialog
        title: "Search Item Master"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: 640
        height: 520

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            TextField {
                id: poPartSearchField
                Layout.fillWidth: true
                placeholderText: "Search by part name, part no, department or vendor..."
                selectByMouse: true
                onTextChanged: refreshPoPartPicker(text)
                // Enter picks the top hit, so a search can be done without
                // ever leaving the keyboard.
                Keys.onReturnPressed: {
                    if (poPartPickerModel.count > 0)
                        choosePoPart(poPartPickerModel.get(0).partName)
                }
            }

            Label {
                text: poPartPickerModel.count + (poPartPickerModel.count === 1 ? " item" : " items")
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 5

                Label {
                    anchors.centerIn: parent
                    visible: poPartPickerModel.count === 0
                    text: "No matching items"
                    color: Theme.textMuted
                }

                ListView {
                    id: poPartPickerView
                    anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: poPartPickerModel
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        width: poPartPickerView.width - 10; height: 58
                        color: partPickerMouse.containsMouse ? "#eaf4fd" : "#fff"
                        border.color: Theme.borderSubtle; radius: 4

                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 8; spacing: 2
                            Label {
                                text: model.partName + (model.partNo !== "" ? "  (" + model.partNo + ")" : "")
                                font.bold: true; font.pixelSize: 13; color: Theme.textPrimary
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                            Label {
                                text: "Dept: " + (model.department !== "" ? model.department : "-")
                                      + "  |  Vendor: " + (model.vendor !== "" ? model.vendor : "-")
                                      + "  |  " + Format.rupees(model.unitPrice)
                                font.pixelSize: 11; color: Theme.textSecondary
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: partPickerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: choosePoPart(model.partName)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: poVendorPickerDialog
        title: "Search Vendors"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: 640
        height: 520

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            TextField {
                id: poVendorSearchField
                Layout.fillWidth: true
                placeholderText: "Search by name, contact person, phone, email or GSTIN..."
                selectByMouse: true
                onTextChanged: refreshPoVendorPicker(text)
                Keys.onReturnPressed: {
                    if (poVendorPickerModel.count > 0)
                        chooseVendorFromPicker(poVendorPickerModel.get(0).name)
                }
            }

            Label {
                text: poVendorPickerModel.count + (poVendorPickerModel.count === 1 ? " vendor" : " vendors")
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 5

                Label {
                    anchors.centerIn: parent
                    visible: poVendorPickerModel.count === 0
                    text: "No matching vendors"
                    color: Theme.textMuted
                }

                ListView {
                    id: poVendorPickerView
                    anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: poVendorPickerModel
                    ScrollBar.vertical: ScrollBar {}
                    delegate: Rectangle {
                        width: poVendorPickerView.width - 10; height: 58
                        color: vendorPickerMouse.containsMouse ? "#eaf4fd" : "#fff"
                        border.color: Theme.borderSubtle; radius: 4

                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 8; spacing: 2
                            Label {
                                text: model.name
                                font.bold: true; font.pixelSize: 13; color: Theme.textPrimary
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                            Label {
                                text: (model.contactPerson !== "" ? model.contactPerson + "  |  " : "")
                                      + (model.phone !== "" ? model.phone : "-")
                                      + (model.email !== "" ? "  |  " + model.email : "")
                                font.pixelSize: 11; color: Theme.textSecondary
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: vendorPickerMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: chooseVendorFromPicker(model.name)
                        }
                    }
                }
            }
        }
    }

    PoPartDetailsDialog {
        id: poPartDetailsDialog
        part: root.selectedPoPartDetails
    }

    // Shared by every date field in the PO screens; openDatePicker() below sets
    // which field the pick lands in and what to seed the calendar with.
    DatePickerDialog { id: expectedDateDialog }

    // Rows for the current status filter, plus the poNo -> searchable-text map
    // from the backend. Both are fetched once per refresh so typing in the
    // search box neither re-queries nor rebuilds any strings per keystroke.
    property var poRowsCache: []
    property var poSearchIndex: ({})

    function refreshPOList() {
        var filter = poFilterCombo.currentText === "All" ? "" : poFilterCombo.currentText
        poRowsCache = Backend.getPOList(filter)
        poSearchIndex = Backend.getPOSearchIndex()
        applyPoSearch()
    }

    // Each whitespace-separated word must appear somewhere in the order, so
    // extra words narrow the result down instead of widening it: "acme bolt"
    // wants both, and "2026-08 draft" is a date-plus-status search.
    function applyPoSearch() {
        var terms = poSearchField.text.toLowerCase().trim().split(/\s+/)
        poListModel.clear()
        for (var i = 0; i < poRowsCache.length; i++) {
            var haystack = poSearchIndex[poRowsCache[i].poNo] || ""
            var matches = true
            for (var t = 0; t < terms.length; t++) {
                if (terms[t] === "") continue
                if (haystack.indexOf(terms[t]) === -1) { matches = false; break }
            }
            if (matches) poListModel.append(poRowsCache[i])
        }
    }

    /* ================= PURCHASE REQUEST (PR) ================= */

    // What is being asked for in the request being written, and the queue of
    // requests everyone can see.
    ListModel { id: prCartModel }
    ListModel { id: prListModel }

    property var prRowsCache: []
    property var prSearchIndex: ({})

    // Empty while writing a new request; the request number while editing one.
    property string editingPRNumber: ""
    property var selectedPRDetails: ({})
    property var selectedPRItems: []
    property string prPendingDelete: ""
    property string prPendingReview: ""
    property string prReviewAction: ""

    // Set while a purchase order is being raised for a request, so the two can
    // be linked the moment the order actually exists.
    property string poFromRequest: ""

    function prCartValue() {
        var t = 0
        for (var i = 0; i < prCartModel.count; i++) {
            var it = prCartModel.get(i)
            t += it.qty * it.estimatedPrice
        }
        return t
    }

    // Fills the blank line fields from the Item Master, so asking for a
    // catalogued part means typing its name and how many, and nothing else.
    function applyPrItemDetails(itemName) {
        var key = (itemName || "").toString().trim()
        if (key === "") return
        var details = poPartDetailsLookup[key]
        if (details === undefined || details === null) return
        prPartNoField.text = details.partNo || ""
        prUnitField.text = details.unit || ""
        prEstPriceField.text = Format.fixed(details.unitPrice)
        if ((details.vendor || "").toString().trim() !== "")
            prVendorField.editText = details.vendor
    }

    function clearPrLineFields() {
        prItemNameField.currentIndex = -1
        prItemNameField.editText = ""
        prPartNoField.text = ""
        prQtyField.value = 1
        prUnitField.text = ""
        prEstPriceField.text = "0.00"
        prVendorField.currentIndex = -1
        prVendorField.editText = ""
    }

    function addPrItemToCart() {
        var itemName = (prItemNameField.editText || prItemNameField.currentText || "").toString().trim()
        if (itemName === "") {
            statusLabel.text = "Enter what you need first"; statusTimer.restart(); return
        }
        prCartModel.append({
            itemName: itemName,
            partNo: prPartNoField.text.trim(),
            qty: prQtyField.value,
            unit: prUnitField.text.trim(),
            estimatedPrice: Format.amount(prEstPriceField.text),
            vendor: (prVendorField.editText || prVendorField.currentText || "").toString().trim()
        })
        clearPrLineFields()
    }

    function clearPrForm() {
        editingPRNumber = ""
        prCartModel.clear()
        clearPrLineFields()
        prDepartmentField.text = ""
        prNeededByField.text = ""
        prPriorityCombo.currentIndex = 0
        prRemarksField.text = ""
        prRequestedByField.text = Backend.currentUser
    }

    // Pulls a pending request back into the form so the person who raised it
    // can correct it before anyone reviews it.
    function loadPrForEdit(prNo) {
        var pr = Backend.getPRByNumber(prNo)
        if (!pr || pr.prNo === undefined) return

        clearPrForm()
        editingPRNumber = prNo
        prDepartmentField.text = pr.department || ""
        prNeededByField.text = pr.neededBy || ""
        prPriorityCombo.currentIndex = (pr.priority === "Urgent") ? 1 : 0
        prRemarksField.text = pr.remarks || ""
        prRequestedByField.text = pr.requestedBy || ""

        var lines = Backend.getPRItems(prNo)
        for (var i = 0; i < lines.length; i++) {
            prCartModel.append({
                itemName: lines[i].itemName || "",
                partNo: lines[i].partNo || "",
                qty: lines[i].qty || 0,
                unit: lines[i].unit || "",
                estimatedPrice: lines[i].estimatedPrice || 0,
                vendor: lines[i].vendor || ""
            })
        }
    }

    function prCartItems() {
        var items = []
        for (var i = 0; i < prCartModel.count; i++) {
            var it = prCartModel.get(i)
            items.push({
                itemName: it.itemName, partNo: it.partNo, qty: it.qty,
                unit: it.unit, estimatedPrice: it.estimatedPrice, vendor: it.vendor
            })
        }
        return items
    }

    function prFormPayload() {
        return {
            department: prDepartmentField.text,
            neededBy: prNeededByField.text,
            priority: prPriorityCombo.currentText,
            remarks: prRemarksField.text,
            requestedBy: prRequestedByField.text
        }
    }

    function submitPurchaseRequest() {
        var items = prCartItems()
        if (editingPRNumber !== "") {
            if (!Backend.updatePurchaseRequest(editingPRNumber, prFormPayload(), items)) return
            statusLabel.text = editingPRNumber + " updated"
            statusTimer.restart()
            clearPrForm()
            refreshPRList()
            return
        }

        var created = Backend.createPurchaseRequest(prFormPayload(), items)
        if (created === "") return
        clearPrForm()
        refreshPRList()
    }

    function refreshPRList() {
        var filter = prFilterCombo.currentText === "All" ? "" : prFilterCombo.currentText
        prRowsCache = Backend.getPRList(filter)
        prSearchIndex = Backend.getPRSearchIndex()
        applyPrSearch()
    }

    // Each whitespace-separated word must appear somewhere in the request, so
    // extra words narrow the result down instead of widening it.
    function applyPrSearch() {
        var terms = prSearchField.text.toLowerCase().trim().split(/\s+/)
        prListModel.clear()
        for (var i = 0; i < prRowsCache.length; i++) {
            var haystack = prSearchIndex[prRowsCache[i].prNo] || ""
            var matches = true
            for (var t = 0; t < terms.length; t++) {
                if (terms[t] === "") continue
                if (haystack.indexOf(terms[t]) === -1) { matches = false; break }
            }
            if (matches) prListModel.append(prRowsCache[i])
        }
    }

    // Hands an approved request over to the purchase order form: its lines
    // become the order's cart, and the two are linked once the order is made.
    function raisePOForRequest(prNo) {
        var lines = Backend.getPRItems(prNo)
        if (lines.length === 0) {
            statusLabel.text = prNo + " has no items"; statusTimer.restart(); return
        }

        poCartModel.clear()
        for (var i = 0; i < lines.length; i++) {
            var l = lines[i]
            poCartModel.append({
                partName: l.itemName || "",
                partNo: l.partNo || "",
                vendor: l.vendor || "",
                department: Backend.getPRByNumber(prNo).department || "",
                qty: l.qty || 0,
                unitPrice: l.estimatedPrice || 0,
                lineTotal: (l.qty || 0) * (l.estimatedPrice || 0)
            })
        }
        recalcPoCartTotal()

        var pr = Backend.getPRByNumber(prNo)
        poExpectedField.text = pr.neededBy || ""
        poExpectedEndField.text = ""
        poFromRequest = prNo

        prDialog.close()
        poDialog.open()
        statusLabel.text = "Raising a purchase order for " + prNo +
                           " - check the vendor and rate on each line"
        statusTimer.restart()
    }

    Dialog {
        id: prDialog
        title: "Purchase Requests"
        modal: true
        anchors.centerIn: parent
        width: parent ? Math.min(1180, parent.width - 60) : 1180
        height: parent ? Math.min(880, parent.height - 40) : 880

        ColumnLayout {
            anchors.fill: parent; spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Purchase Requests"; font.bold: true; font.pixelSize: 16; color: Theme.textPrimary }
                Item { Layout.fillWidth: true }
                Label {
                    text: loginDialog.isAuthenticated
                          ? "Everyone can see this queue. Supply chain approves a request, then raises the order."
                          : "Log in to raise or review a request."
                    font.pixelSize: 10; color: Theme.textMuted
                }
            }

            // ---- Who needs it, and by when ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: prEntryGrid.implicitHeight + 2 * prEntryGrid.anchors.margins
                color: "#fdf5ea"; border.color: Theme.warning; radius: 5

                GridLayout {
                    id: prEntryGrid
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 14
                    columns: 4; rowSpacing: 10; columnSpacing: 14

                    Label {
                        text: "REQUEST DETAILS & APPROVAL CONTEXT"
                        Layout.columnSpan: 4; Layout.fillWidth: true
                        font.bold: true; font.pixelSize: 11; color: Theme.textSecondary
                    }

                    Label {
                        text: editingPRNumber === "" ? "Next Request:" : "Editing:"
                        font.bold: true
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label {
                            text: editingPRNumber === "" ? Backend.getNextPRNumber() : editingPRNumber
                            color: Theme.warning; font.bold: true
                        }
                        Button {
                            text: "Cancel edit"
                            visible: editingPRNumber !== ""
                            flat: true
                            onClicked: clearPrForm()
                            contentItem: Text { text: "Cancel edit"; color: Theme.danger; font.pixelSize: 11 }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        text: "Needed by:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        TextField {
                            id: prNeededByField
                            Layout.preferredWidth: 130
                            placeholderText: "YYYY-MM-DD"
                            readOnly: true
                            selectByMouse: true
                        }
                        Button {
                            text: "\u{1F4C5}"
                            Layout.preferredWidth: 38
                            ToolTip.visible: hovered
                            ToolTip.text: "Pick the date it is needed by"
                            onClicked: openDatePicker(prNeededByField, "needed-by date")
                        }
                        Button {
                            text: "✕"
                            Layout.preferredWidth: 32
                            enabled: prNeededByField.text !== ""
                            ToolTip.visible: hovered
                            ToolTip.text: "Clear the date"
                            onClicked: prNeededByField.text = ""
                        }
                        Label { text: "Priority:"; Layout.leftMargin: 8 }
                        ComboBox {
                            id: prPriorityCombo
                            Layout.preferredWidth: 110
                            model: ["Normal", "Urgent"]
                        }
                    }

                    Label {
                        text: "Requested by:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: prRequestedByField
                        Layout.fillWidth: true
                        text: Backend.currentUser
                        placeholderText: "Who is asking"
                        selectByMouse: true
                    }
                    Label {
                        text: "Department:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: prDepartmentField
                        Layout.fillWidth: true
                        placeholderText: "Which department needs it"
                        selectByMouse: true
                    }

                    Label {
                        text: "Reason / Remarks:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextArea {
                        id: prRemarksField
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        placeholderText: "What it is for - the reviewer reads this before approving"
                        wrapMode: TextEdit.Wrap
                        selectByMouse: true
                        background: Rectangle { color: Theme.surface; border.color: Theme.border; radius: Theme.radiusSmall }
                    }
                }
            }

            // ---- What is being asked for ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: prLineGrid.implicitHeight + 2 * prLineGrid.anchors.margins
                color: "#fffaf4"; border.color: Theme.warning; radius: 5

                GridLayout {
                    id: prLineGrid
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 12
                    columns: 4; rowSpacing: 10; columnSpacing: 14

                    Label {
                        text: "ITEMS REQUESTED"
                        Layout.columnSpan: 4; Layout.fillWidth: true
                        font.bold: true; font.pixelSize: 11; color: Theme.textSecondary
                    }

                    Label {
                        text: "Item Name*:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        ComboBox {
                            id: prItemNameField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            editable: true
                            model: []
                            onCurrentTextChanged: applyPrItemDetails(currentText)
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search the Item Master"
                            onClicked: openPrPartPicker()
                        }
                    }
                    Label {
                        text: "Part No:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField { id: prPartNoField; Layout.fillWidth: true; placeholderText: "If it is a catalogued part"; selectByMouse: true }

                    Label {
                        text: "Quantity* / Unit:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        SpinBox { id: prQtyField; from: 1; to: 100000; value: 1; editable: true; Layout.preferredWidth: 130 }
                        TextField {
                            id: prUnitField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 80
                            placeholderText: "Nos, Kg, Mtr..."
                            selectByMouse: true
                        }
                    }
                    Label {
                        text: "Preferred Vendor:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        ComboBox {
                            id: prVendorField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 160
                            editable: true
                            model: Backend.getVendorNames()
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search vendors"
                            onClicked: openVendorPicker(prVendorField)
                        }
                    }

                    Label {
                        text: "Est. Unit Price:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "₹"; font.bold: true; color: Theme.textPrimary }
                        TextField {
                            id: prEstPriceField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 140
                            placeholderText: "0.00"
                            text: "0.00"
                            horizontalAlignment: TextInput.AlignRight
                            validator: DoubleValidator { bottom: 0; decimals: 2 }
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            selectByMouse: true
                        }
                    }
                    Item {}
                    Button {
                        text: "+ Add Item"
                        highlighted: true
                        Layout.minimumWidth: 120
                        Layout.alignment: Qt.AlignRight
                        onClicked: addPrItemToCart()
                    }
                }
            }

            // Cart: the lines this request is asking for.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: prCartModel.count === 0
                                        ? 58
                                        : Math.min(170, 44 + prCartModel.count * 38)
                border.color: Theme.warning; radius: 5; color: "#ffffff"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6; Layout.rightMargin: 6
                        spacing: 10
                        visible: prCartModel.count > 0
                        Label { text: "Item"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.fillWidth: true }
                        Label { text: "Preferred Vendor"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 170 }
                        Label { text: "Qty"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                        Label { text: "Unit"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 70 }
                        Label { text: "Est. Amount"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 160; horizontalAlignment: Text.AlignRight }
                        Item { Layout.preferredWidth: 30 }
                    }

                    ListView {
                        id: prCartView
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; spacing: 5
                        model: prCartModel
                        delegate: Rectangle {
                            width: prCartView.width; height: 32
                            color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 6
                                anchors.topMargin: 3; anchors.bottomMargin: 3
                                spacing: 10
                                Label {
                                    text: (index + 1) + ". " + model.itemName
                                    font.bold: true; font.pixelSize: 11
                                    Layout.fillWidth: true; Layout.minimumWidth: 120
                                    elide: Text.ElideRight
                                }
                                Label {
                                    text: model.vendor !== "" ? model.vendor : "-"
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.preferredWidth: 170; elide: Text.ElideRight
                                }
                                Label { text: model.qty; font.pixelSize: 11; color: Theme.textPrimary; Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight }
                                Label { text: model.unit; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 70; elide: Text.ElideRight }
                                Label {
                                    text: Format.rupees(model.qty * model.estimatedPrice)
                                    font.pixelSize: 12; font.bold: true; color: Theme.warning
                                    Layout.preferredWidth: 160; horizontalAlignment: Text.AlignRight
                                }
                                Button {
                                    Layout.preferredWidth: 30; Layout.preferredHeight: 24
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Remove this line"
                                    onClicked: prCartModel.remove(index)
                                    contentItem: Text { text: "X"; color: Theme.danger; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "Nothing requested yet - use \"+ Add Item\" above"
                    visible: prCartModel.count === 0; color: Theme.textMuted; font.pixelSize: 11
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 12
                Item { Layout.fillWidth: true }

                Rectangle {
                    Layout.preferredWidth: 280
                    Layout.preferredHeight: 38
                    color: "#fdf0e2"; border.color: Theme.warning; radius: 4
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10
                        Label { text: "Estimated"; font.bold: true; color: Theme.textPrimary; font.pixelSize: 12 }
                        Label {
                            text: Format.rupees(prCartValue())
                            font.bold: true; font.pixelSize: 15; color: Theme.warning
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }

                Button {
                    text: editingPRNumber === ""
                          ? "Send Request (" + prCartModel.count + (prCartModel.count === 1 ? " item)" : " items)")
                          : "Update " + editingPRNumber
                    highlighted: true
                    enabled: prCartModel.count > 0 && loginDialog.isAuthenticated
                    ToolTip.visible: hovered && !loginDialog.isAuthenticated
                    ToolTip.text: "Log in so the request carries your name"
                    onClicked: submitPurchaseRequest()
                }
            }

            // ---- Status filter + free-text search across the queue ----
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 2; spacing: 10

                Label { text: "Filter:"; font.bold: true }
                ComboBox {
                    id: prFilterCombo
                    Layout.preferredWidth: 160
                    model: ["All", "Pending", "Approved", "Rejected", "Ordered"]
                    onCurrentTextChanged: refreshPRList()
                }

                Label { text: "Search:"; font.bold: true; Layout.leftMargin: 6 }
                TextField {
                    id: prSearchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 240
                    placeholderText: "Request number, requester, department, item, or date"
                    selectByMouse: true
                    onTextChanged: applyPrSearch()
                    Keys.onEscapePressed: function(event) {
                        if (text !== "") text = ""
                        else event.accepted = false
                    }
                }
                Button {
                    text: "✕"
                    Layout.preferredWidth: 34
                    enabled: prSearchField.text !== ""
                    ToolTip.visible: hovered
                    ToolTip.text: "Clear the search"
                    onClicked: prSearchField.text = ""
                }
                Label {
                    text: prSearchField.text.trim() === ""
                          ? prListModel.count + (prListModel.count === 1 ? " request" : " requests")
                          : prListModel.count + " of " + prRowsCache.length + " match"
                    color: Theme.textSecondary; font.pixelSize: 11
                }
                Label {
                    text: "Pending: " + Backend.pendingRequestCount
                    font.bold: true; color: Theme.warning
                }
            }

            // ---- The queue ----
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 90
                border.color: Theme.border; radius: 5

                ListView {
                    id: prListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: prListModel
                    delegate: Rectangle {
                        width: prListView.width - 10; height: 74
                        color: {
                            if (model.status === "Approved") return "#eaf6ff"
                            if (model.status === "Ordered") return Theme.successWash
                            if (model.status === "Rejected") return "#f7eaea"
                            return Theme.warningWash
                        }
                        border.color: Theme.borderSubtle; radius: 4

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 200; spacing: 3
                                RowLayout {
                                    spacing: 8
                                    Label { text: model.prNo; font.bold: true; font.pixelSize: 13; color: Theme.textPrimary }
                                    Label {
                                        text: model.status
                                        font.pixelSize: 11; font.bold: true
                                        color: {
                                            if (model.status === "Approved") return "#2a78d6"
                                            if (model.status === "Ordered") return Theme.success
                                            if (model.status === "Rejected") return Theme.danger
                                            return Theme.pending
                                        }
                                    }
                                    Label {
                                        text: "URGENT"
                                        visible: model.priority === "Urgent"
                                        font.pixelSize: 10; font.bold: true; color: Theme.danger
                                    }
                                    Label {
                                        text: "→ " + model.poNo
                                        visible: (model.poNo || "") !== ""
                                        font.pixelSize: 11; color: Theme.success; font.bold: true
                                    }
                                }
                                Label {
                                    text: model.summary + "  |  Qty: " + model.totalQty +
                                          "  |  Est: " + Format.rupees(model.estimatedValue)
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Label {
                                    text: "By: " + (model.requestedBy || "-") +
                                          (model.department !== "" ? "  |  " + model.department : "") +
                                          "  |  Raised: " + model.date +
                                          (model.neededBy !== "" ? "  |  Needed by: " + model.neededBy : "") +
                                          ((model.reviewedBy || "") !== "" ? "  |  Reviewed by: " + model.reviewedBy : "")
                                    font.pixelSize: 10; color: Theme.textMuted
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                            }

                            Button {
                                text: "View"
                                onClicked: {
                                    selectedPRDetails = Backend.getPRByNumber(model.prNo)
                                    selectedPRItems = Backend.getPRItems(model.prNo)
                                    prDetailsDialog.open()
                                }
                                contentItem: Text { text: "View"; color: Theme.textPrimary; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Approve"
                                visible: model.status === "Pending"
                                enabled: loginDialog.isAuthenticated
                                onClicked: {
                                    prPendingReview = model.prNo
                                    prReviewAction = "Approved"
                                    prReviewNoteField.text = ""
                                    prReviewDialog.open()
                                }
                                contentItem: Text { text: "Approve"; color: parent.enabled ? Theme.success : "#95a5a6"; font.pixelSize: 11; font.bold: true }
                            }
                            Button {
                                text: "Reject"
                                visible: model.status === "Pending"
                                enabled: loginDialog.isAuthenticated
                                onClicked: {
                                    prPendingReview = model.prNo
                                    prReviewAction = "Rejected"
                                    prReviewNoteField.text = ""
                                    prReviewDialog.open()
                                }
                                contentItem: Text { text: "Reject"; color: parent.enabled ? Theme.danger : "#95a5a6"; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Create PO"
                                visible: model.status === "Approved"
                                enabled: loginDialog.isAuthenticated
                                ToolTip.visible: hovered
                                ToolTip.text: "Open the purchase order form with these items"
                                onClicked: raisePOForRequest(model.prNo)
                                contentItem: Text { text: "Create PO"; color: parent.enabled ? Theme.info : "#95a5a6"; font.pixelSize: 11; font.bold: true }
                            }
                            Button {
                                text: "Edit"
                                visible: model.status === "Pending"
                                enabled: loginDialog.isAuthenticated
                                onClicked: loadPrForEdit(model.prNo)
                                contentItem: Text { text: "Edit"; color: parent.enabled ? "#8e44ad" : "#95a5a6"; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Delete"
                                visible: (model.poNo || "") === ""
                                enabled: loginDialog.isAuthenticated
                                onClicked: { prPendingDelete = model.prNo; prDeleteConfirmDialog.open() }
                                contentItem: Text { text: "Delete"; color: parent.enabled ? Theme.danger : "#95a5a6"; font.pixelSize: 11 }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: prSearchField.text.trim() === ""
                          ? "No purchase requests yet"
                          : "No requests match \"" + prSearchField.text.trim() + "\""
                    visible: prListModel.count === 0; color: Theme.textMuted
                }
            }

            Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: prDialog.close() }
        }

        onOpened: {
            prSearchField.text = ""
            if (editingPRNumber === "" && prCartModel.count === 0)
                clearPrForm()
            refreshPartNameDropdown()
            refreshVendorDropdowns()
            refreshPRList()
        }
    }

    Dialog {
        id: prReviewDialog
        title: prReviewAction === "Approved" ? "Approve " + prPendingReview
                                             : "Reject " + prPendingReview
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        width: 480

        ColumnLayout {
            anchors.fill: parent; spacing: 10
            Label {
                text: prReviewAction === "Approved"
                      ? "Approving lets supply chain raise a purchase order for this request."
                      : "Rejecting closes the request. Say why, so the requester knows."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            Label { text: "Note (optional):" }
            TextField {
                id: prReviewNoteField
                Layout.fillWidth: true
                placeholderText: prReviewAction === "Approved" ? "e.g. buy from the usual vendor"
                                                               : "e.g. already in stock"
                selectByMouse: true
            }
        }

        onAccepted: {
            if (Backend.setPurchaseRequestStatus(prPendingReview, prReviewAction,
                                                      Backend.currentUser,
                                                      prReviewNoteField.text)) {
                statusLabel.text = prPendingReview + " " + prReviewAction.toLowerCase()
                statusTimer.restart()
                refreshPRList()
            }
            prPendingReview = ""
        }
    }

    Dialog {
        id: prDeleteConfirmDialog
        title: "Delete Purchase Request"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        width: 460

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Delete " + prPendingDelete + " and its items? The request number will not be reused."
        }

        onAccepted: {
            if (Backend.deletePurchaseRequest(prPendingDelete)) {
                if (editingPRNumber === prPendingDelete) clearPrForm()
                statusLabel.text = "Deleted " + prPendingDelete
                statusTimer.restart()
                refreshPRList()
            }
            prPendingDelete = ""
        }
    }

    /* ---- Item Master picker for a request line ---- */

    ListModel { id: prPartPickerModel }

    function refreshPrPartPicker(filter) {
        prPartPickerModel.clear()
        var f = (filter || "").toString().trim().toLowerCase()
        var items = Backend.getItemMasterList()
        for (var i = 0; i < items.length; i++) {
            var it = items[i]
            var partName = (it.partName || "").toString()
            if (partName.trim() === "") continue
            var partNo = (it.partNo || "").toString()
            var dept = (it.department || it.category || "").toString()
            var vendor = (it.vendor || "").toString()
            if (f !== "" &&
                (partName + " " + partNo + " " + dept + " " + vendor).toLowerCase().indexOf(f) === -1)
                continue
            prPartPickerModel.append({
                partName: partName, partNo: partNo, department: dept, vendor: vendor,
                unit: (it.unit || "").toString(), unitPrice: it.unitPrice || 0
            })
        }
    }

    function openPrPartPicker() {
        prPartSearchField.text = ""
        refreshPrPartPicker("")
        prPartPickerDialog.open()
        prPartSearchField.forceActiveFocus()
    }

    function choosePrPart(item) {
        var idx = prItemNameField.find(item.partName)
        if (idx >= 0) prItemNameField.currentIndex = idx
        else prItemNameField.editText = item.partName
        prPartNoField.text = item.partNo
        prUnitField.text = item.unit
        prEstPriceField.text = Format.fixed(item.unitPrice)
        if (item.vendor !== "") prVendorField.editText = item.vendor
        prPartPickerDialog.close()
    }

    Dialog {
        id: prPartPickerDialog
        title: "Search Item Master"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: Math.min(root.width - 120, 720)
        height: Math.min(root.height - 140, 520)

        ColumnLayout {
            anchors.fill: parent; spacing: 8

            TextField {
                id: prPartSearchField
                Layout.fillWidth: true
                placeholderText: "Item name, part number, department or vendor"
                selectByMouse: true
                onTextChanged: refreshPrPartPicker(text)
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 4

                ListView {
                    id: prPartPickerView
                    anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: prPartPickerModel
                    delegate: Rectangle {
                        width: prPartPickerView.width - 10; height: 46
                        color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: choosePrPart({
                                partName: model.partName, partNo: model.partNo,
                                unit: model.unit, unitPrice: model.unitPrice,
                                vendor: model.vendor
                            })
                        }
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 2
                            Label { text: model.partName; font.bold: true; font.pixelSize: 12; color: Theme.textPrimary }
                            Label {
                                text: (model.partNo !== "" ? "Part No: " + model.partNo : "No part number") +
                                      (model.department !== "" ? "  |  " + model.department : "") +
                                      (model.vendor !== "" ? "  |  " + model.vendor : "") +
                                      "  |  " + Format.rupees(model.unitPrice)
                                font.pixelSize: 10; color: Theme.textSecondary
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No matching items"
                    visible: prPartPickerModel.count === 0; color: Theme.textMuted
                }
            }
        }
    }

    /* ---- Request details ---- */

    PrDetailsDialog {
        id: prDetailsDialog
        request: root.selectedPRDetails
        lineItems: root.selectedPRItems
    }

    /* ================= STOCK ROW EDITOR ================= */

    // The grid itself is read-only, so a correction goes through here: one
    // deliberate action, login-gated, with every column of the row in view.
    property int editingStockRow: -1

    function openStockRowEditor(row) {
        if (row <= 0 || !loginDialog.isAuthenticated) return
        editingStockRow = row
        stockEditPartName.text = (Backend.model.getData(row, 0) || "").toString()
        stockEditPartNo.text = (Backend.model.getData(row, 1) || "").toString()
        stockEditStock.text = (Backend.model.getData(row, 2) || "").toString()
        stockEditDepartment.text = (Backend.model.getData(row, 3) || "").toString()
        stockEditPrepared.text = (Backend.model.getData(row, 4) || "").toString()
        stockEditApproved.text = (Backend.model.getData(row, 5) || "").toString()
        stockEditVendor.text = (Backend.model.getData(row, 6) || "").toString()
        stockEditDate.text = (Backend.model.getData(row, 7) || "").toString()
        stockEditUnitPrice.text = (Backend.model.getData(row, 8) || "").toString()
        stockRowEditDialog.open()
    }

    Dialog {
        id: stockRowEditDialog
        title: "Edit Stock Row " + editingStockRow
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Save | Dialog.Cancel
        width: 620

        onAccepted: {
            if (editingStockRow <= 0) return
            var m = Backend.model
            m.setDataAt(editingStockRow, 0, stockEditPartName.text)
            m.setDataAt(editingStockRow, 1, stockEditPartNo.text)
            m.setDataAt(editingStockRow, 2, stockEditStock.text)
            m.setDataAt(editingStockRow, 3, stockEditDepartment.text)
            m.setDataAt(editingStockRow, 4, stockEditPrepared.text)
            m.setDataAt(editingStockRow, 5, stockEditApproved.text)
            m.setDataAt(editingStockRow, 6, stockEditVendor.text)
            m.setDataAt(editingStockRow, 7, stockEditDate.text)
            m.setDataAt(editingStockRow, 8, stockEditUnitPrice.text)
            // An edit can move a part between departments or change a
            // quantity, so re-roll the totals.
            root.tableRefreshToken++
            refreshStockOverview()
            statusLabel.text = "Row " + editingStockRow + " updated"
            statusTimer.restart()
        }

        GridLayout {
            anchors.fill: parent
            columns: 4; rowSpacing: 8; columnSpacing: 10

            Label { text: "Part Name:" }
            TextField { id: stockEditPartName; Layout.fillWidth: true; selectByMouse: true }
            Label { text: "Part No:" }
            TextField { id: stockEditPartNo; Layout.fillWidth: true; selectByMouse: true }

            Label { text: "Stock:" }
            TextField {
                id: stockEditStock; Layout.fillWidth: true
                horizontalAlignment: TextInput.AlignRight
                validator: DoubleValidator { bottom: 0; decimals: 2 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
            }
            Label { text: "Department:" }
            TextField { id: stockEditDepartment; Layout.fillWidth: true; selectByMouse: true }

            Label { text: "Prepared:" }
            TextField { id: stockEditPrepared; Layout.fillWidth: true; selectByMouse: true }
            Label { text: "Approved:" }
            TextField { id: stockEditApproved; Layout.fillWidth: true; selectByMouse: true }

            Label { text: "Vendor Name:" }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                TextField { id: stockEditVendor; Layout.fillWidth: true; selectByMouse: true }
                Button {
                    text: "\u{1F50D}"
                    Layout.preferredWidth: 40
                    ToolTip.visible: hovered
                    ToolTip.text: "Search vendors"
                    onClicked: openVendorPicker(stockEditVendor)
                }
            }
            Label { text: "Date:" }
            RowLayout {
                Layout.fillWidth: true; spacing: 6
                TextField { id: stockEditDate; Layout.fillWidth: true; placeholderText: "YYYY-MM-DD"; selectByMouse: true }
                Button {
                    text: "\u{1F4C5}"
                    Layout.preferredWidth: 38
                    ToolTip.visible: hovered
                    ToolTip.text: "Pick the date"
                    onClicked: openDatePicker(stockEditDate, "row date")
                }
            }

            Label { text: "Unit Price:" }
            TextField {
                id: stockEditUnitPrice; Layout.fillWidth: true
                horizontalAlignment: TextInput.AlignRight
                validator: DoubleValidator { bottom: 0; decimals: 2 }
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                selectByMouse: true
            }
            Item { Layout.columnSpan: 2 }
        }
    }

    /* ================= DELIVERY CHALLAN (DC) ================= */

    // Items being collected for the challan being written (the "cart"), and the
    // list of challans already raised.
    ListModel { id: dcCartModel }
    ListModel { id: dcListModel }
    ListModel { id: dcPartyPickerModel }

    // Both are fetched once per refresh so typing in the search box neither
    // re-queries nor rebuilds any strings per keystroke.
    property var dcRowsCache: []
    property var dcSearchIndex: ({})

    // Empty while writing a new challan; the challan number while editing one.
    property string editingDCNumber: ""
    property var selectedDCDetails: ({})
    property var selectedDCItems: []

    property string dcPreviewDcNo: ""
    property var dcPreviewPages: []
    property string dcPreviewPdfPath: ""
    property string dcLastSavedPdf: ""

    function dcCartQty() {
        var t = 0
        for (var i = 0; i < dcCartModel.count; i++) t += dcCartModel.get(i).qty
        return t
    }

    // Fills the blank line fields from the Item Master, exactly as the PO entry
    // does, so an item's part number, HSN code and unit are typed once ever.
    function applyDcItemDetails(itemName) {
        var key = (itemName || "").toString().trim()
        if (key === "") return
        var details = poPartDetailsLookup[key]
        if (details === undefined || details === null) return
        dcPartNoField.text = details.partNo || ""
        dcHsnField.text = details.hsnCode || ""
        dcUnitField.text = details.unit || ""
    }

    function clearDcLineFields() {
        dcItemNameField.currentIndex = -1
        dcItemNameField.editText = ""
        dcPartNoField.text = ""
        dcHsnField.text = ""
        dcQtyField.text = "1"
        dcUnitField.text = ""
    }

    function addDcItemToCart() {
        var itemName = (dcItemNameField.editText || dcItemNameField.currentText || "").toString().trim()
        if (itemName === "") {
            statusLabel.text = "Enter an item name first"; statusTimer.restart(); return
        }
        var qty = Format.amount(dcQtyField.text)
        if (qty <= 0) {
            statusLabel.text = "Quantity must be greater than 0 for " + itemName
            statusTimer.restart(); return
        }
        dcCartModel.append({
            itemName: itemName,
            partNo: dcPartNoField.text.trim(),
            hsnCode: dcHsnField.text.trim(),
            qty: qty,
            unit: dcUnitField.text.trim()
        })
        clearDcLineFields()
    }

    function clearDcForm() {
        editingDCNumber = ""
        dcCartModel.clear()
        clearDcLineFields()
        dcDateField.text = Qt.formatDate(new Date(), "yyyy-MM-dd")
        dcDeliveryTimeField.text = ""
        dcPartyNameField.text = ""
        dcPartyAddressField.text = ""
        dcPartyPhoneField.text = ""
        dcPartyEmailField.text = ""
        dcPartyGstinField.text = ""
        dcSeparateShipping.checked = false
        dcShipNameField.text = ""
        dcShipAddressField.text = ""
        dcShipPhoneField.text = ""
        dcShipEmailField.text = ""
        dcShipGstinField.text = ""
        dcTermsField.text = ""
        dcDeliveredByField.text = Backend.currentUser
        dcReceivedByField.text = ""
    }

    // Pulls a draft challan back into the entry form so it can be corrected
    // without burning a challan number.
    function loadDcForEdit(dcNo) {
        var dc = Backend.getDCByNumber(dcNo)
        if (!dc || dc.dcNo === undefined) return

        clearDcForm()
        editingDCNumber = dcNo
        dcDateField.text = dc.date || ""
        dcDeliveryTimeField.text = dc.deliveryTime || ""
        dcPartyNameField.text = dc.partyName || ""
        dcPartyAddressField.text = dc.partyAddress || ""
        dcPartyPhoneField.text = dc.partyPhone || ""
        dcPartyEmailField.text = dc.partyEmail || ""
        dcPartyGstinField.text = dc.partyGstin || ""
        dcTermsField.text = dc.terms || ""
        dcDeliveredByField.text = dc.deliveredBy || ""
        dcReceivedByField.text = dc.receivedBy || ""

        // Only show the shipping block when the goods really go somewhere else;
        // a consignee equal to the party was filled in for us on creation.
        var differs = (dc.shipName || "") !== (dc.partyName || "") ||
                      (dc.shipAddress || "") !== (dc.partyAddress || "")
        dcSeparateShipping.checked = differs
        if (differs) {
            dcShipNameField.text = dc.shipName || ""
            dcShipAddressField.text = dc.shipAddress || ""
            dcShipPhoneField.text = dc.shipPhone || ""
            dcShipEmailField.text = dc.shipEmail || ""
            dcShipGstinField.text = dc.shipGstin || ""
        }

        var lines = Backend.getDCItems(dcNo)
        for (var i = 0; i < lines.length; i++) {
            dcCartModel.append({
                itemName: lines[i].itemName || "",
                partNo: lines[i].partNo || "",
                hsnCode: lines[i].hsnCode || "",
                qty: lines[i].qty || 0,
                unit: lines[i].unit || ""
            })
        }
    }

    function dcFormPayload() {
        return {
            date: dcDateField.text,
            deliveryTime: dcDeliveryTimeField.text,
            partyName: dcPartyNameField.text,
            partyAddress: dcPartyAddressField.text,
            partyPhone: dcPartyPhoneField.text,
            partyEmail: dcPartyEmailField.text,
            partyGstin: dcPartyGstinField.text,
            shipName: dcSeparateShipping.checked ? dcShipNameField.text : "",
            shipAddress: dcSeparateShipping.checked ? dcShipAddressField.text : "",
            shipPhone: dcSeparateShipping.checked ? dcShipPhoneField.text : "",
            shipEmail: dcSeparateShipping.checked ? dcShipEmailField.text : "",
            shipGstin: dcSeparateShipping.checked ? dcShipGstinField.text : "",
            terms: dcTermsField.text,
            deliveredBy: dcDeliveredByField.text,
            receivedBy: dcReceivedByField.text,
            preparedBy: Backend.currentUser
        }
    }

    function dcCartItems() {
        var items = []
        for (var i = 0; i < dcCartModel.count; i++) {
            var it = dcCartModel.get(i)
            items.push({
                itemName: it.itemName, partNo: it.partNo, hsnCode: it.hsnCode,
                qty: it.qty, unit: it.unit
            })
        }
        return items
    }

    function submitDeliveryChallan() {
        var items = dcCartItems()
        if (editingDCNumber !== "") {
            var dcNo = editingDCNumber
            if (!Backend.updateDeliveryChallan(dcNo, dcFormPayload(), items)) return
            clearDcForm()
            refreshDCList()
            openDcPdfPreview(dcNo)
            return
        }

        var created = Backend.createDeliveryChallan(dcFormPayload(), items)
        if (created === "") return
        clearDcForm()
        refreshDCList()
        openDcPdfPreview(created)
    }

    function refreshDCList() {
        var filter = dcFilterCombo.currentText === "All" ? "" : dcFilterCombo.currentText
        dcRowsCache = Backend.getDCList(filter)
        dcSearchIndex = Backend.getDCSearchIndex()
        applyDcSearch()
    }

    // Each whitespace-separated word must appear somewhere in the challan, so
    // extra words narrow the result down instead of widening it.
    function applyDcSearch() {
        var terms = dcSearchField.text.toLowerCase().trim().split(/\s+/)
        dcListModel.clear()
        for (var i = 0; i < dcRowsCache.length; i++) {
            var haystack = dcSearchIndex[dcRowsCache[i].dcNo] || ""
            var matches = true
            for (var t = 0; t < terms.length; t++) {
                if (terms[t] === "") continue
                if (haystack.indexOf(terms[t]) === -1) { matches = false; break }
            }
            if (matches) dcListModel.append(dcRowsCache[i])
        }
    }

    // ---- Party picker: everyone this company has delivered to, plus the
    // vendors, since goods going back to a supplier travel on a challan too.
    function refreshDcPartyPicker(filter) {
        dcPartyPickerModel.clear()
        var f = (filter || "").toString().trim().toLowerCase()
        var seen = ({})

        var parties = Backend.getDCPartyList()
        for (var i = 0; i < parties.length; i++) {
            var p = parties[i]
            var name = (p.partyName || "").toString()
            if (name.trim() === "") continue
            seen[name.toLowerCase()] = true
            if (f !== "" && (name + " " + (p.partyAddress || "") + " " + (p.partyGstin || "") +
                             " " + (p.partyPhone || "")).toLowerCase().indexOf(f) === -1) continue
            dcPartyPickerModel.append({
                name: name, address: (p.partyAddress || "").toString(),
                phone: (p.partyPhone || "").toString(), email: (p.partyEmail || "").toString(),
                gstin: (p.partyGstin || "").toString(), source: "Delivered before"
            })
        }

        var vendors = Backend.getVendorList()
        for (var v = 0; v < vendors.length; v++) {
            var vn = (vendors[v].vendorName || "").toString()
            if (vn.trim() === "" || seen[vn.toLowerCase()]) continue
            var addr = (vendors[v].vendorAddress || "").toString()
            var gstin = (vendors[v].gstin || "").toString()
            var phone = (vendors[v].phone || "").toString()
            if (f !== "" && (vn + " " + addr + " " + gstin + " " + phone).toLowerCase().indexOf(f) === -1)
                continue
            dcPartyPickerModel.append({
                name: vn, address: addr, phone: phone,
                email: (vendors[v].email || "").toString(), gstin: gstin, source: "Vendor"
            })
        }
    }

    function openDcPartyPicker() {
        dcPartySearchField.text = ""
        refreshDcPartyPicker("")
        dcPartyPickerDialog.open()
        dcPartySearchField.forceActiveFocus()
    }

    function chooseDcParty(party) {
        dcPartyNameField.text = party.name
        dcPartyAddressField.text = party.address
        dcPartyPhoneField.text = party.phone
        dcPartyEmailField.text = party.email
        dcPartyGstinField.text = party.gstin
        dcPartyPickerDialog.close()
    }

    // ---- Printable delivery challan ----

    function openDcPdfPreview(dcNo) {
        dcPreviewDcNo = dcNo
        refreshDcPdfPreview()
        if (dcPreviewPages.length > 0)
            dcPdfPreviewDialog.open()
    }

    function refreshDcPdfPreview() {
        var result = Backend.generateDCPreview(dcPreviewDcNo)
        if (!result || !result.pages || result.pages.length === 0) {
            dcPreviewPages = []
            dcPreviewPdfPath = ""
            statusLabel.text = "Could not render the delivery challan PDF"
            statusTimer.restart()
            return
        }
        dcPreviewPages = result.pages
        dcPreviewPdfPath = result.pdfPath
    }

    function sendDcPdf() {
        var saved = Backend.saveDCPdf(dcPreviewDcNo, "")
        if (saved === "") return
        dcLastSavedPdf = saved
        statusLabel.text = "Delivery challan saved to " + saved
        statusTimer.restart()
        dcPdfPreviewDialog.close()
        dcSavedDialog.open()
    }

    Dialog {
        id: dcDialog
        title: "Delivery Challan"
        modal: true
        anchors.centerIn: parent
        width: parent ? Math.min(1180, parent.width - 60) : 1180
        height: parent ? Math.min(880, parent.height - 40) : 880

        ColumnLayout {
            anchors.fill: parent; spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Label { text: "Delivery Challans"; font.bold: true; font.pixelSize: 16; color: Theme.textPrimary }
                Item { Layout.fillWidth: true }
                Label {
                    text: "A challan records what left the premises. It does not change stock levels — use Issue Stock for that."
                    font.pixelSize: 10; color: Theme.textMuted
                }
            }

            // ---- Who it goes to, and when ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: dcEntryGrid.implicitHeight + 2 * dcEntryGrid.anchors.margins
                color: "#eef8fd"; border.color: "#1b9dd9"; radius: 5

                GridLayout {
                    id: dcEntryGrid
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 14
                    columns: 4; rowSpacing: 10; columnSpacing: 14

                    Label {
                        text: "PARTY, DELIVERY & CONSIGNEE DETAILS"
                        Layout.columnSpan: 4; Layout.fillWidth: true
                        font.bold: true; font.pixelSize: 11; color: Theme.textSecondary
                    }

                    Label {
                        text: editingDCNumber === "" ? "Next Challan:" : "Editing:"
                        font.bold: true
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        Label {
                            text: editingDCNumber === "" ? Backend.getNextDCNumber() : editingDCNumber
                            color: "#1b9dd9"; font.bold: true
                        }
                        Button {
                            text: "Cancel edit"
                            visible: editingDCNumber !== ""
                            flat: true
                            onClicked: clearDcForm()
                            contentItem: Text { text: "Cancel edit"; color: Theme.danger; font.pixelSize: 11 }
                        }
                        Item { Layout.fillWidth: true }
                    }
                    Label {
                        text: "Date:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        TextField {
                            id: dcDateField
                            Layout.preferredWidth: 130
                            placeholderText: "YYYY-MM-DD"
                            readOnly: true
                            text: Qt.formatDate(new Date(), "yyyy-MM-dd")
                            selectByMouse: true
                        }
                        Button {
                            text: "\u{1F4C5}"
                            Layout.preferredWidth: 38
                            ToolTip.visible: hovered
                            ToolTip.text: "Pick the challan date"
                            onClicked: openDatePicker(dcDateField, "challan date")
                        }
                        Label { text: "Delivery Time:"; Layout.leftMargin: 8 }
                        TextField {
                            id: dcDeliveryTimeField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 110
                            placeholderText: "e.g. 04:30 PM"
                            selectByMouse: true
                        }
                    }

                    Label {
                        text: "Party Name*:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        TextField {
                            id: dcPartyNameField
                            Layout.fillWidth: true
                            placeholderText: "Who the goods are being delivered to"
                            selectByMouse: true
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search parties delivered to before, and vendors"
                            onClicked: openDcPartyPicker()
                        }
                    }
                    Label {
                        text: "GSTIN:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField { id: dcPartyGstinField; Layout.fillWidth: true; placeholderText: "Party GSTIN"; selectByMouse: true }

                    Label {
                        text: "Address:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    }
                    TextArea {
                        id: dcPartyAddressField
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        placeholderText: "Street, city - PIN (one line per printed line)"
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        background: Rectangle { color: "#ffffff"; border.color: Theme.divider; radius: 3 }
                    }
                    Label {
                        text: "Phone / Email:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        TextField { id: dcPartyPhoneField; Layout.fillWidth: true; placeholderText: "Phone number"; selectByMouse: true }
                        TextField { id: dcPartyEmailField; Layout.fillWidth: true; placeholderText: "Email ID"; selectByMouse: true }
                    }

                    CheckBox {
                        id: dcSeparateShipping
                        text: "Shipping to a different address"
                        Layout.columnSpan: 4
                        ToolTip.visible: hovered
                        ToolTip.text: "Leave off to ship to the party's own address"
                    }

                    Label {
                        text: "Shipping Name:"; visible: dcSeparateShipping.checked
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: dcShipNameField; visible: dcSeparateShipping.checked
                        Layout.fillWidth: true; placeholderText: "Consignee name"; selectByMouse: true
                    }
                    Label {
                        text: "Shipping GSTIN:"; visible: dcSeparateShipping.checked
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: dcShipGstinField; visible: dcSeparateShipping.checked
                        Layout.fillWidth: true; placeholderText: "Consignee GSTIN"; selectByMouse: true
                    }

                    Label {
                        text: "Shipping Address:"; visible: dcSeparateShipping.checked
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignTop
                    }
                    TextArea {
                        id: dcShipAddressField; visible: dcSeparateShipping.checked
                        Layout.fillWidth: true
                        Layout.preferredHeight: 52
                        placeholderText: "Where the goods are actually going"
                        wrapMode: TextArea.Wrap
                        selectByMouse: true
                        background: Rectangle { color: "#ffffff"; border.color: Theme.divider; radius: 3 }
                    }
                    Label {
                        text: "Phone / Email:"; visible: dcSeparateShipping.checked
                        horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    ColumnLayout {
                        visible: dcSeparateShipping.checked
                        Layout.fillWidth: true; spacing: 6
                        TextField { id: dcShipPhoneField; Layout.fillWidth: true; placeholderText: "Phone number"; selectByMouse: true }
                        TextField { id: dcShipEmailField; Layout.fillWidth: true; placeholderText: "Email ID"; selectByMouse: true }
                    }
                }
            }

            // ---- What is being delivered ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: dcLineGrid.implicitHeight + 2 * dcLineGrid.anchors.margins
                color: "#f7fbfd"; border.color: "#1b9dd9"; radius: 5

                GridLayout {
                    id: dcLineGrid
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 12
                    columns: 4; rowSpacing: 10; columnSpacing: 14

                    Label {
                        text: "ITEMS BEING DELIVERED"
                        Layout.columnSpan: 4; Layout.fillWidth: true
                        font.bold: true; font.pixelSize: 11; color: Theme.textSecondary
                    }

                    Label {
                        text: "Item Name*:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        ComboBox {
                            id: dcItemNameField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            editable: true
                            model: []
                            onCurrentTextChanged: applyDcItemDetails(currentText)
                        }
                        Button {
                            text: "\u{1F50D}"
                            Layout.preferredWidth: 40
                            ToolTip.visible: hovered
                            ToolTip.text: "Search the Item Master"
                            onClicked: openDcPartPicker()
                        }
                    }
                    Label {
                        text: "Part No:"
                        Layout.minimumWidth: 104; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField { id: dcPartNoField; Layout.fillWidth: true; placeholderText: "Printed under the item name"; selectByMouse: true }

                    Label {
                        text: "HSN/SAC Code:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    TextField {
                        id: dcHsnField; Layout.fillWidth: true
                        placeholderText: "Auto-filled from the Item Master"
                        selectByMouse: true
                    }
                    Label {
                        text: "Quantity* / Unit:"; horizontalAlignment: Text.AlignRight
                        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        TextField {
                            id: dcQtyField
                            Layout.preferredWidth: 110
                            text: "1"
                            horizontalAlignment: TextInput.AlignRight
                            validator: DoubleValidator { bottom: 0; decimals: 2 }
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            selectByMouse: true
                            Keys.onReturnPressed: addDcItemToCart()
                        }
                        TextField {
                            id: dcUnitField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 90
                            placeholderText: "Nos, Kg, Mtr..."
                            selectByMouse: true
                            Keys.onReturnPressed: addDcItemToCart()
                        }
                        Button {
                            text: "+ Add Item"
                            highlighted: true
                            Layout.minimumWidth: 110
                            onClicked: addDcItemToCart()
                        }
                    }
                }
            }

            // Cart: the lines that will be printed on this challan.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: dcCartModel.count === 0
                                        ? 58
                                        : Math.min(180, 44 + dcCartModel.count * 38)
                border.color: "#1b9dd9"; radius: 5; color: "#ffffff"

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 8; spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 6; Layout.rightMargin: 6
                        spacing: 10
                        visible: dcCartModel.count > 0
                        Label { text: "Item"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.fillWidth: true }
                        Label { text: "Part No"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 160 }
                        Label { text: "HSN/SAC"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 130 }
                        Label { text: "Qty"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight }
                        Label { text: "Unit"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 90 }
                        Item { Layout.preferredWidth: 30 }
                    }

                    ListView {
                        id: dcCartView
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true; spacing: 5
                        model: dcCartModel
                        delegate: Rectangle {
                            width: dcCartView.width; height: 32
                            color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6; anchors.rightMargin: 6
                                anchors.topMargin: 3; anchors.bottomMargin: 3
                                spacing: 10
                                Label {
                                    text: (index + 1) + ". " + model.itemName
                                    font.bold: true; font.pixelSize: 11
                                    Layout.fillWidth: true; Layout.minimumWidth: 120
                                    elide: Text.ElideRight
                                }
                                Label { text: model.partNo; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 160; elide: Text.ElideRight }
                                Label { text: model.hsnCode; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 130; elide: Text.ElideRight }
                                Label { text: Format.qty(model.qty); font.pixelSize: 12; font.bold: true; color: "#1b9dd9"; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight }
                                Label { text: model.unit; font.pixelSize: 11; color: Theme.textPrimary; Layout.preferredWidth: 90; elide: Text.ElideRight }
                                Button {
                                    Layout.preferredWidth: 30; Layout.preferredHeight: 24
                                    ToolTip.visible: hovered
                                    ToolTip.text: "Remove this line"
                                    onClicked: dcCartModel.remove(index)
                                    contentItem: Text { text: "X"; color: Theme.danger; font.pixelSize: 11; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No items added - use \"+ Add Item\" above"
                    visible: dcCartModel.count === 0; color: Theme.textMuted; font.pixelSize: 11
                }
            }

            // ---- Terms, who handed over, and the create button ----
            RowLayout {
                Layout.fillWidth: true; spacing: 12

                Label { text: "Terms &amp; Conditions:"; Layout.alignment: Qt.AlignTop; Layout.topMargin: 6 }
                TextArea {
                    id: dcTermsField
                    Layout.fillWidth: true
                    Layout.preferredHeight: 58
                    placeholderText: "Printed at the foot of the challan"
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    background: Rectangle { color: "#ffffff"; border.color: Theme.divider; radius: 3 }
                }

                ColumnLayout {
                    Layout.preferredWidth: 280; spacing: 6
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Delivered By:"; Layout.preferredWidth: 84 }
                        TextField { id: dcDeliveredByField; Layout.fillWidth: true; text: Backend.currentUser; selectByMouse: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        Label { text: "Received By:"; Layout.preferredWidth: 84 }
                        TextField { id: dcReceivedByField; Layout.fillWidth: true; placeholderText: "Left blank to sign on paper"; selectByMouse: true }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: 210; spacing: 4
                    Label {
                        text: "Total quantity: " + Format.qty(dcCartQty())
                        font.bold: true; font.pixelSize: 13; color: "#1b9dd9"
                        Layout.alignment: Qt.AlignRight
                    }
                    Button {
                        text: editingDCNumber === ""
                              ? "Create Challan (" + dcCartModel.count + (dcCartModel.count === 1 ? " item)" : " items)")
                              : "Update " + editingDCNumber
                        highlighted: true
                        enabled: dcCartModel.count > 0 && dcPartyNameField.text.trim() !== ""
                        Layout.fillWidth: true
                        onClicked: submitDeliveryChallan()
                    }
                }
            }

            // ---- Status filter + free-text search across the challan list ----
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 2; spacing: 10

                Label { text: "Filter:"; font.bold: true }
                ComboBox {
                    id: dcFilterCombo
                    Layout.preferredWidth: 160
                    model: ["All", "Draft", "Delivered", "Cancelled"]
                    onCurrentTextChanged: refreshDCList()
                }

                Label { text: "Search:"; font.bold: true; Layout.leftMargin: 6 }
                TextField {
                    id: dcSearchField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 240
                    placeholderText: "Challan number, party, item, or date (e.g. 2026-08)"
                    selectByMouse: true
                    onTextChanged: applyDcSearch()
                    Keys.onEscapePressed: function(event) {
                        if (text !== "") text = ""
                        else event.accepted = false
                    }
                }
                Button {
                    text: "✕"
                    Layout.preferredWidth: 34
                    enabled: dcSearchField.text !== ""
                    ToolTip.visible: hovered
                    ToolTip.text: "Clear the search"
                    onClicked: dcSearchField.text = ""
                }
                Label {
                    text: dcSearchField.text.trim() === ""
                          ? dcListModel.count + (dcListModel.count === 1 ? " challan" : " challans")
                          : dcListModel.count + " of " + dcRowsCache.length + " match"
                    color: Theme.textSecondary; font.pixelSize: 11
                }
            }

            // ---- The challans already raised ----
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                Layout.minimumHeight: 90
                border.color: Theme.border; radius: 5

                ListView {
                    id: dcListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: dcListModel
                    delegate: Rectangle {
                        width: dcListView.width - 10; height: 68
                        color: {
                            if (model.status === "Delivered") return Theme.successWash
                            if (model.status === "Cancelled") return "#f4f4f4"
                            return Theme.warningWash
                        }
                        border.color: Theme.borderSubtle; radius: 4

                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10; spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true; Layout.minimumWidth: 200; spacing: 3
                                RowLayout {
                                    spacing: 8
                                    Label { text: model.dcNo; font.bold: true; font.pixelSize: 13; color: Theme.textPrimary }
                                    Label {
                                        text: model.status
                                        font.pixelSize: 11; font.bold: true
                                        color: {
                                            if (model.status === "Delivered") return Theme.success
                                            if (model.status === "Cancelled") return Theme.textMuted
                                            return Theme.pending
                                        }
                                    }
                                }
                                Label {
                                    text: "To: " + model.partyName +
                                          "  |  " + model.summary +
                                          "  |  Qty: " + Format.qty(model.totalQty)
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Label {
                                    text: "Date: " + model.date +
                                          (model.deliveryTime !== "" ? "  |  Time: " + model.deliveryTime : "") +
                                          "  |  Delivered by: " + (model.deliveredBy || "-")
                                    font.pixelSize: 10; color: Theme.textMuted
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                            }

                            Button {
                                text: "View"
                                onClicked: {
                                    selectedDCDetails = Backend.getDCByNumber(model.dcNo)
                                    selectedDCItems = Backend.getDCItems(model.dcNo)
                                    dcDetailsDialog.open()
                                }
                                contentItem: Text { text: "View"; color: Theme.textPrimary; font.pixelSize: 11 }
                            }
                            Button {
                                text: "PDF"
                                ToolTip.visible: hovered
                                ToolTip.text: "Preview and save the printed challan"
                                onClicked: openDcPdfPreview(model.dcNo)
                                contentItem: Text { text: "PDF"; color: "#1b9dd9"; font.pixelSize: 11; font.bold: true }
                            }
                            Button {
                                text: "Edit"
                                visible: model.status === "Draft"
                                onClicked: loadDcForEdit(model.dcNo)
                                contentItem: Text { text: "Edit"; color: "#8e44ad"; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Delivered"
                                visible: model.status === "Draft"
                                onClicked: { Backend.updateDCStatus(model.dcNo, "Delivered"); refreshDCList() }
                                contentItem: Text { text: "Delivered"; color: Theme.success; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Cancel"
                                visible: model.status !== "Cancelled"
                                onClicked: { Backend.updateDCStatus(model.dcNo, "Cancelled"); refreshDCList() }
                                contentItem: Text { text: "Cancel"; color: Theme.warning; font.pixelSize: 11 }
                            }
                            Button {
                                text: "Delete"
                                onClicked: { dcPendingDelete = model.dcNo; dcDeleteConfirmDialog.open() }
                                contentItem: Text { text: "Delete"; color: Theme.danger; font.pixelSize: 11 }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    text: dcSearchField.text.trim() === ""
                          ? "No delivery challans yet"
                          : "No challans match \"" + dcSearchField.text.trim() + "\""
                    visible: dcListModel.count === 0; color: Theme.textMuted
                }
            }

            Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: dcDialog.close() }
        }

        onOpened: {
            dcSearchField.text = ""
            if (editingDCNumber === "" && dcCartModel.count === 0)
                clearDcForm()
            refreshPartNameDropdown()
            refreshDCList()
        }
    }

    property string dcPendingDelete: ""

    Dialog {
        id: dcDeleteConfirmDialog
        title: "Delete Delivery Challan"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        width: 460

        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: "Delete " + dcPendingDelete + " and its items? The challan number will not be reused."
        }

        onAccepted: {
            if (Backend.deleteDeliveryChallan(dcPendingDelete)) {
                if (editingDCNumber === dcPendingDelete) clearDcForm()
                statusLabel.text = "Deleted " + dcPendingDelete
                statusTimer.restart()
                refreshDCList()
            }
            dcPendingDelete = ""
        }
    }

    /* ---- Item Master picker for a challan line ---- */

    ListModel { id: dcPartPickerModel }

    function refreshDcPartPicker(filter) {
        dcPartPickerModel.clear()
        var f = (filter || "").toString().trim().toLowerCase()
        var items = Backend.getItemMasterList()
        for (var i = 0; i < items.length; i++) {
            var it = items[i]
            var partName = (it.partName || "").toString()
            if (partName.trim() === "") continue
            var partNo = (it.partNo || "").toString()
            var hsn = (it.taxCode || it.hsnCode || "").toString()
            var unit = (it.unit || "").toString()
            if (f !== "" &&
                (partName + " " + partNo + " " + hsn + " " + unit).toLowerCase().indexOf(f) === -1)
                continue
            dcPartPickerModel.append({
                partName: partName, partNo: partNo, hsnCode: hsn, unit: unit
            })
        }
    }

    function openDcPartPicker() {
        dcPartSearchField.text = ""
        refreshDcPartPicker("")
        dcPartPickerDialog.open()
        dcPartSearchField.forceActiveFocus()
    }

    function chooseDcPart(item) {
        var idx = dcItemNameField.find(item.partName)
        if (idx >= 0) dcItemNameField.currentIndex = idx
        else dcItemNameField.editText = item.partName
        dcPartNoField.text = item.partNo
        dcHsnField.text = item.hsnCode
        dcUnitField.text = item.unit
        dcPartPickerDialog.close()
    }

    Dialog {
        id: dcPartPickerDialog
        title: "Search Item Master"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: Math.min(root.width - 120, 720)
        height: Math.min(root.height - 140, 520)

        ColumnLayout {
            anchors.fill: parent; spacing: 8

            TextField {
                id: dcPartSearchField
                Layout.fillWidth: true
                placeholderText: "Item name, part number, HSN code or unit"
                selectByMouse: true
                onTextChanged: refreshDcPartPicker(text)
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 4

                ListView {
                    id: dcPartPickerView
                    anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: dcPartPickerModel
                    delegate: Rectangle {
                        width: dcPartPickerView.width - 10; height: 46
                        color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: chooseDcPart({
                                partName: model.partName, partNo: model.partNo,
                                hsnCode: model.hsnCode, unit: model.unit
                            })
                        }
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 2
                            Label { text: model.partName; font.bold: true; font.pixelSize: 12; color: Theme.textPrimary }
                            Label {
                                text: (model.partNo !== "" ? "Part No: " + model.partNo : "No part number") +
                                      (model.hsnCode !== "" ? "  |  HSN: " + model.hsnCode : "") +
                                      (model.unit !== "" ? "  |  Unit: " + model.unit : "")
                                font.pixelSize: 10; color: Theme.textSecondary
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No matching items"
                    visible: dcPartPickerModel.count === 0; color: Theme.textMuted
                }
            }
        }
    }

    /* ---- Party picker ---- */

    Dialog {
        id: dcPartyPickerDialog
        title: "Search Parties"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Cancel
        width: Math.min(root.width - 120, 720)
        height: Math.min(root.height - 140, 520)

        ColumnLayout {
            anchors.fill: parent; spacing: 8

            TextField {
                id: dcPartySearchField
                Layout.fillWidth: true
                placeholderText: "Party name, address, phone or GSTIN"
                selectByMouse: true
                onTextChanged: refreshDcPartyPicker(text)
            }

            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                border.color: Theme.border; radius: 4

                ListView {
                    id: dcPartyPickerView
                    anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                    model: dcPartyPickerModel
                    delegate: Rectangle {
                        width: dcPartyPickerView.width - 10; height: 58
                        color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: chooseDcParty({
                                name: model.name, address: model.address,
                                phone: model.phone, email: model.email, gstin: model.gstin
                            })
                        }
                        ColumnLayout {
                            anchors.fill: parent; anchors.margins: 6; spacing: 2
                            RowLayout {
                                spacing: 8
                                Label { text: model.name; font.bold: true; font.pixelSize: 12; color: Theme.textPrimary }
                                Label { text: model.source; font.pixelSize: 10; color: "#1b9dd9" }
                            }
                            Label {
                                text: model.address.replace(/\n/g, ", ")
                                font.pixelSize: 10; color: Theme.textSecondary
                                Layout.fillWidth: true; elide: Text.ElideRight
                            }
                            Label {
                                text: (model.phone !== "" ? model.phone : "") +
                                      (model.gstin !== "" ? "  |  GSTIN: " + model.gstin : "")
                                font.pixelSize: 10; color: Theme.textMuted
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: "No matching parties. Type the details in directly - they are remembered for next time."
                    visible: dcPartyPickerModel.count === 0; color: Theme.textMuted
                }
            }
        }
    }

    /* ---- Challan details ---- */

    Dialog {
        id: dcDetailsDialog
        title: "Delivery Challan " + (selectedDCDetails.dcNo || "")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: Math.min(root.width - 120, 720)

        ColumnLayout {
            anchors.fill: parent; spacing: 8

            GridLayout {
                Layout.fillWidth: true
                columns: 4; rowSpacing: 4; columnSpacing: 10

                Label { text: "Challan No:"; font.bold: true }
                Label { text: selectedDCDetails.dcNo || "-"; Layout.fillWidth: true }
                Label { text: "Status:"; font.bold: true }
                Label { text: selectedDCDetails.status || "-"; Layout.fillWidth: true }

                Label { text: "Date:"; font.bold: true }
                Label { text: selectedDCDetails.date || "-"; Layout.fillWidth: true }
                Label { text: "Delivery Time:"; font.bold: true }
                Label { text: selectedDCDetails.deliveryTime || "-"; Layout.fillWidth: true }

                Label { text: "Party:"; font.bold: true }
                Label { text: selectedDCDetails.partyName || "-"; Layout.fillWidth: true; elide: Text.ElideRight }
                Label { text: "Party GSTIN:"; font.bold: true }
                Label { text: selectedDCDetails.partyGstin || "-"; Layout.fillWidth: true }

                Label { text: "Address:"; font.bold: true; Layout.alignment: Qt.AlignTop }
                Label {
                    text: (selectedDCDetails.partyAddress || "-").replace(/\n/g, ", ")
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }
                Label { text: "Shipping To:"; font.bold: true; Layout.alignment: Qt.AlignTop }
                Label {
                    text: (selectedDCDetails.shipName || "-") +
                          ((selectedDCDetails.shipAddress || "") !== ""
                           ? ", " + selectedDCDetails.shipAddress.replace(/\n/g, ", ") : "")
                    Layout.fillWidth: true; wrapMode: Text.WordWrap
                }

                Label { text: "Delivered By:"; font.bold: true }
                Label { text: selectedDCDetails.deliveredBy || "-"; Layout.fillWidth: true }
                Label { text: "Received By:"; font.bold: true }
                Label { text: selectedDCDetails.receivedBy || "-"; Layout.fillWidth: true }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

            Label {
                text: "Items (" + selectedDCItems.length + ")"
                font.bold: true; color: Theme.textPrimary
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(220, 8 + selectedDCItems.length * 26)
                border.color: Theme.border; radius: 4

                ListView {
                    id: dcDetailItemsView
                    anchors.fill: parent; anchors.margins: 4; clip: true
                    model: selectedDCItems
                    delegate: RowLayout {
                        width: dcDetailItemsView.width - 8
                        height: 26
                        spacing: 10
                        Label { text: (index + 1) + ". " + modelData.itemName; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                        Label { text: modelData.hsnCode; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 120 }
                        Label { text: Format.qty(modelData.qty) + " " + modelData.unit; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 110; horizontalAlignment: Text.AlignRight }
                    }
                }
            }

            Button {
                text: "Open the printed challan"
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    dcDetailsDialog.close()
                    openDcPdfPreview(selectedDCDetails.dcNo)
                }
            }
        }
    }

    /* ---- Printed challan preview ---- */

    DcPdfPreviewDialog {
        id: dcPdfPreviewDialog
        dcNo: root.dcPreviewDcNo
        pages: root.dcPreviewPages
        pdfPath: root.dcPreviewPdfPath
        onSendRequested: sendDcPdf()
    }

    DcSavedDialog {
        id: dcSavedDialog
        pdfPath: root.dcLastSavedPdf
    }

    /* ================= GRN (GOODS RECEIPT) DIALOG ================= */

    property int grnRemainingQty: 0

    // Line items of the PO being received.
    ListModel { id: grnItemsModel }

    function grnLoadItems(poNo) {
        grnItemsModel.clear()
        var lines = Backend.getPOItems(poNo)
        var firstOpen = -1
        for (var i = 0; i < lines.length; i++) {
            var l = lines[i]
            grnItemsModel.append({
                itemId: l.id,
                label: l.partName + "  (" + l.receivedQty + "/" + l.qty + " received)",
                qty: l.qty,
                receivedQty: l.receivedQty
            })
            if (firstOpen === -1 && l.receivedQty < l.qty) firstOpen = i
        }
        grnItemCombo.currentIndex = firstOpen >= 0 ? firstOpen : 0
        grnApplySelection(grnItemCombo.currentIndex)
    }

    function grnApplySelection(index) {
        if (index < 0 || index >= grnItemsModel.count) {
            grnOrderQty.text = ""; grnReceivedSoFar.text = ""; grnRemainingQty = 0
            grnReceivedField.value = 0; grnAcceptedField.value = 0
            return
        }
        var line = grnItemsModel.get(index)
        grnOrderQty.text = line.qty.toString()
        grnReceivedSoFar.text = line.receivedQty.toString()
        grnRemainingQty = Math.max(0, line.qty - line.receivedQty)
        grnReceivedField.value = grnRemainingQty
        grnAcceptedField.value = grnRemainingQty
        grnRejectedField.value = 0
    }

    Dialog {
        id: grnDialog
        title: "Receive Goods (GRN)"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        width: 470

        ColumnLayout {
            spacing: 10; width: parent.width

            Label { text: "Goods Receipt Note"; font.bold: true; font.pixelSize: 14; color: Theme.success }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.success }

            GridLayout {
                columns: 2; rowSpacing: 8; columnSpacing: 10; Layout.fillWidth: true

                Label { text: "PO No:"; font.bold: true } Label { id: grnPOLabel; text: grnPOFieldText; font.bold: true; color: Theme.info }
                Label { text: "Item*:"; font.bold: true }
                ComboBox {
                    id: grnItemCombo
                    Layout.fillWidth: true
                    model: grnItemsModel
                    textRole: "label"
                    onActivated: function(index) { grnApplySelection(index) }
                }
                Label { text: "Order Qty:" } Label { id: grnOrderQty; text: "" }
                Label { text: "Already Received:" } Label { id: grnReceivedSoFar; text: "" }
                Label { text: "Remaining:" ; font.bold: true } Label { text: grnRemainingQty.toString(); font.bold: true; color: Theme.warning }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

            Label { text: "Received Qty:" }
            SpinBox { id: grnReceivedField; from: 0; to: 100000; value: 0; editable: true; Layout.fillWidth: true }

            Label { text: "Accepted Qty:" }
            SpinBox { id: grnAcceptedField; from: 0; to: 100000; value: 0; editable: true; Layout.fillWidth: true }

            Label { text: "Rejected Qty:" }
            SpinBox { id: grnRejectedField; from: 0; to: 100000; value: 0; editable: true; Layout.fillWidth: true }

            Label { text: "Received By*:" }
            TextField {
                id: grnReceivedByField
                Layout.fillWidth: true
                text: Backend.currentUser
                selectByMouse: true
            }

            Label { text: "Remarks:" }
            TextField { id: grnRemarksField; Layout.fillWidth: true; placeholderText: "Any remarks..." }
        }

        onAccepted: {
            if (grnItemCombo.currentIndex < 0 || grnItemsModel.count === 0) return
            var itemId = grnItemsModel.get(grnItemCombo.currentIndex).itemId
            var grnNo = Backend.receiveGoodsForItem(
                itemId, grnReceivedField.value,
                grnAcceptedField.value, grnRejectedField.value,
                grnRemarksField.text, grnReceivedByField.text)
            if (grnNo !== "") {
                refreshPOList()
                refreshMovements()
                rows = Backend.model.rowCount()
            }
        }
    }

    property string grnPOFieldText: ""

    /* ================= ISSUE STOCK DIALOG ================= */

    // Parts collected for the next material issue (the "cart").
    ListModel { id: issueCartModel }

    function addIssueItemToCart() {
        var partName = (issuePartField.editText || issuePartField.currentText || "").toString().trim()
        if (partName === "") { statusLabel.text = "Select a part first"; statusTimer.restart(); return }
        // Merge duplicate parts into one line.
        for (var i = 0; i < issueCartModel.count; i++) {
            if (issueCartModel.get(i).partName === partName) {
                issueCartModel.setProperty(i, "qty", issueCartModel.get(i).qty + issueQtyField.value)
                issuePartField.currentIndex = -1; issuePartField.editText = ""; issueQtyField.value = 1
                return
            }
        }
        issueCartModel.append({ partName: partName, qty: issueQtyField.value })
        issuePartField.currentIndex = -1
        issuePartField.editText = ""
        issueQtyField.value = 1
    }

    Dialog {
        id: issueDialog
        title: "Issue Stock to Department"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        anchors.centerIn: parent
        width: 470

        ColumnLayout {
            spacing: 10; width: parent.width

            Label { text: "Material Issue Note"; font.bold: true; font.pixelSize: 14; color: Theme.warning }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.warning }

            Label { text: "Search Part:" }
            TextField {
                id: issueSearchField
                Layout.fillWidth: true
                placeholderText: "Type to filter available parts..."
                selectByMouse: true
                onTextChanged: refreshIssuePartDropdown(text)
            }

            Rectangle {
                Layout.fillWidth: true; height: 100
                border.color: Theme.border; radius: 4; color: Theme.surfaceAlt

                ListView {
                    anchors.fill: parent; anchors.margins: 4; clip: true; spacing: 4
                    model: issueSearchModel
                    delegate: Rectangle {
                        width: parent.width; height: 28
                        color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 6
                            Label { text: model.name; font.pixelSize: 12; font.bold: true; color: Theme.textPrimary; Layout.fillWidth: true }
                            Label { text: "Stock: " + model.stock; font.pixelSize: 11; color: Theme.success }
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                issuePartField.editText = model.name
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No matching parts"
                    visible: issueSearchModel.count === 0
                    color: Theme.textMuted
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 8
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Label { text: "Part Name*:" }
                    ComboBox {
                        id: issuePartField
                        Layout.fillWidth: true
                        editable: true
                        model: issueSearchModel
                        textRole: "name"
                    }
                }
                ColumnLayout {
                    spacing: 2
                    Label { text: "Quantity*:" }
                    SpinBox { id: issueQtyField; from: 1; to: 100000; value: 1; editable: true }
                }
                ColumnLayout {
                    spacing: 2
                    Label { text: " " }
                    Button { text: "+ Add"; highlighted: true; onClicked: addIssueItemToCart() }
                }
            }

            // Cart: parts that will be issued together under one issue number
            Rectangle {
                Layout.fillWidth: true; height: 96
                border.color: Theme.warning; radius: 4; color: "#fffaf5"

                ListView {
                    id: issueCartView
                    anchors.fill: parent; anchors.margins: 4; clip: true; spacing: 2
                    model: issueCartModel
                    delegate: Rectangle {
                        width: issueCartView.width - 8; height: 24
                        color: "#fff"; border.color: Theme.borderSubtle; radius: 3
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 4
                            Label { text: (index + 1) + ". " + model.partName; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: "Qty: " + model.qty; font.pixelSize: 11; color: Theme.warning }
                            Button {
                                text: "X"
                                Layout.preferredWidth: 24; Layout.preferredHeight: 18
                                onClicked: issueCartModel.remove(index)
                                contentItem: Text { text: "X"; color: Theme.danger; font.pixelSize: 9; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: "No parts added - use \"+ Add\" (several parts can go in one issue)"
                    visible: issueCartModel.count === 0
                    color: Theme.textMuted; font.pixelSize: 11
                }
            }

            Label { text: "Issue To Department*:" }
            ComboBox { id: issueDeptField; Layout.fillWidth: true; editable: true; model: ["Electronics", "Mechanical", "Hardware", "Software", "Production", "R&D", "Other"] }

            Label { text: "Issued By:" }
            TextField { id: issueByField; Layout.fillWidth: true; text: Backend.currentUser; selectByMouse: true }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

            Label { text: "Stock will be deducted automatically for every part in the list"; font.pixelSize: 10; color: Theme.warning; wrapMode: Text.WordWrap }
        }

        onAccepted: {
            // Anything still sitting in the entry fields counts as one more line.
            var pendingPart = (issuePartField.editText || issuePartField.currentText || "").toString().trim()
            if (pendingPart !== "") addIssueItemToCart()

            if (issueCartModel.count === 0) {
                statusLabel.text = "Add at least one part to issue"
                statusTimer.restart()
                return
            }

            var items = []
            for (var i = 0; i < issueCartModel.count; i++) {
                var it = issueCartModel.get(i)
                items.push({ partName: it.partName, qty: it.qty })
            }
            var issueNo = Backend.issueMultipleStock(
                items, issueDeptField.currentText, issueByField.text)
            if (issueNo !== "") {
                issueCartModel.clear()
                issuePartField.currentIndex = -1
                issuePartField.editText = ""
                issueQtyField.value = 1
                rows = Backend.model.rowCount()
                refreshMovements()
            }
        }

        onOpened: {
            issueSearchField.text = ""
            issueCartModel.clear()
            refreshIssuePartDropdown("")
            issueByField.text = Backend.currentUser
        }
    }

    /* ================= STOCK MOVEMENTS DIALOG ================= */

    StockMovementsDialog {
        id: movementsDialog
        onRefreshRequested: refreshMovements()
    }

    /* ================= REPORTS DIALOG ================= */

    ReportsDialog { id: reportsDialog }

    function refreshMovements() {
        movementsModel.clear()
        var movements = Backend.getStockMovements(movFilterField.text)
        for (var i = movements.length - 1; i >= 0; i--) movementsModel.append(movements[i])
    }

    /* ================= LOW STOCK ALERT DIALOG ================= */

    LowStockDialog {
        id: lowStockDialog
        onRefreshRequested: refresh()
        onOrderChanged: refreshPOList()
    }

    function refreshLowStock() {
        lowStockDialog.refresh()
    }

    /* ================= ISSUE NOTES LIST DIALOG ================= */

    IssueNotesDialog { id: issueNotesDialog }

    /* ================= DATABASE CONNECTION DIALOG ================= */

    // True while this computer is talking to a shared server rather than its own
    // local file; gates the one-time "copy my data up" step below.
    property bool dbOnServer: false
    // True when this machine is set up to use the shared server but is running
    // on its own local copy instead, so nothing it does is seen by anyone else.
    property bool dbUsingLocalFallback: false
    property string dbFallbackReason: ""
    property string dbConfiguredHost: ""

    function refreshDbConnectionState() {
        var cfg = Backend.getDatabaseSettings()
        root.dbOnServer = Backend.isDatabaseServerBackend()
        root.dbConfiguredHost = cfg.host || ""
        root.dbUsingLocalFallback = (cfg.driver === "QPSQL") && !root.dbOnServer
        root.dbFallbackReason = Backend.databaseLastError()
    }

    function retryDatabaseConnection() {
        var cfg = Backend.getDatabaseSettings()
        statusLabel.text = "Reconnecting to " + cfg.host + "..."
        Backend.configureDatabase(cfg.driver, cfg.host, cfg.port,
                                       cfg.name, cfg.user, cfg.password)
        refreshDbConnectionState()
        if (root.dbOnServer) {
            rows = Backend.model.rowCount()
            columns = Backend.model.columnCount()
            root.tableRefreshToken++
            refreshStockOverview()
            statusLabel.text = "Connected to the shared database"
        } else {
            statusLabel.text = "Still cannot reach " + cfg.host
        }
        statusTimer.restart()
    }

    Dialog {
        id: cloudSettingsDialog
        title: "Database Connection"
        modal: true
        width: 560
        height: parent ? Math.min(780, parent.height - 60) : 780
        anchors.centerIn: parent

        onOpened: {
            root.dbOnServer = Backend.isDatabaseServerBackend()
            dbCopyResult.text = ""
            var cfg = Backend.getDatabaseSettings()
            dbDriverCombo.currentIndex = (cfg.driver === "QPSQL") ? 1 : 0
            dbHostField.text = cfg.host !== undefined ? cfg.host : ""
            dbPortField.text = (cfg.port !== undefined ? cfg.port : 5432).toString()
            dbNameField.text = cfg.name !== undefined ? cfg.name : "stockmanager"
            dbUserField.text = cfg.user !== undefined ? cfg.user : ""
            dbPassField.text = cfg.password !== undefined ? cfg.password : ""
            dbStatusLabel.text = Backend.databaseStatus()
            serverInfoBox.visible = false
            root.dbSetupBusy = false
        }

        ScrollView {
            id: dbSettingsScroll
            anchors.fill: parent
            contentWidth: availableWidth
            clip: true

        ColumnLayout {
            spacing: 16; width: dbSettingsScroll.availableWidth

            Label {
                text: "All data is stored in one shared database so everyone works on the same\nnumbers. Pick ONE computer to be the server; every other computer connects to it."
                color: Theme.textSecondary; font.pixelSize: 12; Layout.fillWidth: true; wrapMode: Text.WordWrap
            }

            /* -------- Option 1: make THIS computer the server -------- */
            Rectangle {
                Layout.fillWidth: true; radius: 6
                color: "#f4f9ff"; border.color: Theme.info; border.width: 1
                implicitHeight: serverSetupCol.implicitHeight + 24
                ColumnLayout {
                    id: serverSetupCol
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Label { text: "① Make this computer the server"; font.bold: true; color: Theme.textPrimary }
                    Label {
                        text: "Installs and configures the shared database on THIS computer. Do this on\njust one machine (the one that stays on). You'll be asked for your system password."
                        color: Theme.textSecondary; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: 10; Layout.fillWidth: true
                        Button {
                            text: root.dbSetupBusy ? "Setting up..." : "Set up this computer as the server"
                            enabled: !root.dbSetupBusy
                            onClicked: {
                                root.dbSetupBusy = true
                                serverInfoBox.visible = false
                                dbStatusLabel.text = "Starting server setup..."
                                Backend.setupThisComputerAsServer()
                            }
                        }
                        BusyIndicator { running: root.dbSetupBusy; visible: root.dbSetupBusy; implicitWidth: 28; implicitHeight: 28 }
                    }
                    Rectangle {
                        id: serverInfoBox
                        visible: false
                        Layout.fillWidth: true; radius: 4
                        color: "#eafaf1"; border.color: Theme.success; border.width: 1
                        implicitHeight: serverInfoCol.implicitHeight + 20
                        ColumnLayout {
                            id: serverInfoCol
                            anchors.fill: parent; anchors.margins: 10; spacing: 3
                            Label { text: "✓ This computer is now the server. On every OTHER computer, open this"; font.pixelSize: 11; font.bold: true; color: "#1e8449"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Label { text: "same dialog, choose PostgreSQL, and enter:"; font.pixelSize: 11; font.bold: true; color: "#1e8449"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
                            Label { text: "   Host: " + dbHostField.text + "   Port: " + dbPortField.text; font.pixelSize: 11; font.family: "monospace"; color: "#145a32" }
                            Label { text: "   Database: " + dbNameField.text + "   User: " + dbUserField.text; font.pixelSize: 11; font.family: "monospace"; color: "#145a32" }
                            Label { text: "   Password: " + dbPassField.text; font.pixelSize: 11; font.family: "monospace"; color: "#145a32" }
                            Label { text: "Write these down — the password is shown only here."; font.pixelSize: 10; font.italic: true; color: Theme.textSecondary; Layout.topMargin: 4 }
                        }
                    }
                }
            }

            /* -------- Option 2: connect to an existing server -------- */
            Label { text: "② Or connect to a server / choose storage"; font.bold: true; color: Theme.textPrimary }
            ComboBox {
                id: dbDriverCombo; Layout.fillWidth: true
                model: ["SQLite (local file — this computer only)", "PostgreSQL (connect to shared server)"]
            }

            GridLayout {
                columns: 2; rowSpacing: 8; columnSpacing: 10; Layout.fillWidth: true
                enabled: dbDriverCombo.currentIndex === 1
                Label { text: "Host:" }     TextField { id: dbHostField; Layout.fillWidth: true; placeholderText: "192.168.1.10 (the server computer)" }
                Label { text: "Port:" }     TextField { id: dbPortField; Layout.fillWidth: true; placeholderText: "5432"; inputMethodHints: Qt.ImhDigitsOnly }
                Label { text: "Database:" } TextField { id: dbNameField; Layout.fillWidth: true; placeholderText: "stockmanager" }
                Label { text: "User:" }     TextField { id: dbUserField; Layout.fillWidth: true }
                Label { text: "Password:" } TextField { id: dbPassField; Layout.fillWidth: true; echoMode: TextInput.Password }
            }

            Label { id: dbStatusLabel; text: ""; color: Theme.textPrimary; Layout.fillWidth: true; wrapMode: Text.WordWrap }

            /* -------- Option 3: bring existing work onto the server -------- */
            Rectangle {
                Layout.fillWidth: true; radius: 6
                color: root.dbOnServer ? "#fdf6e3" : "#f6f6f6"
                border.color: root.dbOnServer ? Theme.warning : "#dcdcdc"; border.width: 1
                implicitHeight: dbCopyCol.implicitHeight + 24
                ColumnLayout {
                    id: dbCopyCol
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    Label { text: "\u2462 Bring this computer's existing work onto the server"; font.bold: true; color: Theme.textPrimary }
                    Label {
                        text: "Run this ONCE, on the computer that already has your stock, item master and\npurchase orders. It copies them into the shared database so every other computer\nsees them, and carries PO/GRN numbering on from where you are."
                        color: Theme.textSecondary; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                    Label {
                        text: "Connect to the shared server above first \u2014 this step needs a server connection."
                        visible: !root.dbOnServer
                        color: Theme.dangerStrong; font.pixelSize: 11; Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                    Button {
                        text: "Copy this computer's data to the server"
                        enabled: root.dbOnServer && !root.dbSetupBusy
                        onClicked: {
                            var res = Backend.copyLocalDataToServer()
                            dbCopyResult.text = res.message || ""
                            dbCopyResult.isError = !res.success
                            if (res.success) {
                                // The stock grid and PO list are showing the
                                // pre-copy server contents; refresh both.
                                root.rows = Backend.model.rowCount()
                                root.columns = Backend.model.columnCount()
                                root.tableRefreshToken++
                                refreshPOList()
                                statusLabel.text = res.message
                                statusTimer.restart()
                            }
                        }
                    }
                    Label {
                        id: dbCopyResult
                        property bool isError: false
                        text: ""; visible: text !== ""
                        color: isError ? Theme.dangerStrong : "#1e8449"
                        font.pixelSize: 11; font.bold: true
                        Layout.fillWidth: true; wrapMode: Text.WordWrap
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                Button { text: "Close"; enabled: !root.dbSetupBusy; onClicked: cloudSettingsDialog.close() }
                Button {
                    text: "Connect & Save"; highlighted: true; enabled: !root.dbSetupBusy
                    onClicked: {
                        var wantServer = dbDriverCombo.currentIndex === 1
                        var driver = wantServer ? "QPSQL" : "QSQLITE"
                        Backend.configureDatabase(
                            driver, dbHostField.text, parseInt(dbPortField.text) || 5432,
                            dbNameField.text, dbUserField.text, dbPassField.text)

                        // configureDatabase() reports success even when it
                        // silently fell back to a local SQLite file. If the
                        // user asked for the shared server but we did NOT end
                        // up on a server backend, that's a real failure the
                        // user must see — otherwise their edits won't be shared.
                        if (wantServer && !Backend.isDatabaseServerBackend()) {
                            dbStatusLabel.text =
                                "Could NOT connect to PostgreSQL — using a LOCAL file, so data will NOT be shared.\n"
                                + "Reason: " + Backend.databaseLastError()
                            statusLabel.text = "PostgreSQL connection failed (using local file)"
                        } else {
                            dbStatusLabel.text = Backend.databaseStatus()
                            statusLabel.text = wantServer ? "Connected to shared database"
                                                          : "Using local database (this computer only)"
                            refreshDbConnectionState()
                            // Stay open after joining a server so the one-time
                            // data copy below is right where it is needed.
                            if (!wantServer) cloudSettingsDialog.close()
                        }
                    }
                }
            }
        }
        }
    }

    /* ================= TOOLBAR ================= */

    header: ToolBar {
        background: Rectangle { color: "#34495e" }

        RowLayout {
            anchors.fill: parent; spacing: 3

            // FILE OPERATIONS
            // ToolButton {
            //     text: "New Stock"
            //     onClicked: newStockFileDialog.open()
            //     ToolTip.visible: hovered; ToolTip.text: "Create new stock file"
            //     contentItem: Text { text: parent.text; color: Theme.success; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            // }

            ToolButton {
                text: "Open"
                onClicked: openExcelFile()
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Import Stock"
                enabled: loginDialog.isAuthenticated
                onClicked: importStockFileAction()
                ToolTip.visible: hovered; ToolTip.text: "Import a stock xlsx into the shared database (login required)"
                contentItem: Text { text: parent.text; color: parent.enabled ? Theme.pending : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Save"
                enabled: Backend.currentFile !== "" && loginDialog.isAuthenticated
                onClicked: Backend.saveExcel("")
                contentItem: Text { text: parent.text; color: parent.enabled ? "white" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Save As"
                enabled: loginDialog.isAuthenticated
                onClicked: saveAsExcelFile()
                contentItem: Text { text: parent.text; color: parent.enabled ? "white" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolSeparator { contentItem: Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.textSecondary } }

            // SUPPLY CHAIN BUTTONS
            ToolButton {
                text: "Vendor Master"
                onClicked: vendorDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Manage Vendors"
                contentItem: Text { text: parent.text; color: "#1abc9c"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Item Master"
                onClicked: itemMasterDialog.open()
                ToolTip.visible:hovered; ToolTip.text: "Manage Item"
                contentItem: Text { text: parent.text; color: "#0ed0d6"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: {
                    var count = Backend.pendingRequestCount
                    return "Purchase Requests" + (count > 0 ? " (" + count + ")" : "")
                }
                onClicked: prDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Ask for something to be bought, and see every open request"
                contentItem: Text {
                    text: parent.text
                    color: Backend.pendingRequestCount > 0 ? Theme.warning : "#f1c40f"
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    font.bold: true; font.pixelSize: 11
                }
            }

            ToolButton {
                text: "Purchase Orders"
                enabled: loginDialog.isAuthenticated
                onClicked: poDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Create/manage purchase orders"
                contentItem: Text { text: parent.text; color: parent.enabled ? Theme.info : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Delivery Challan"
                enabled: loginDialog.isAuthenticated
                onClicked: dcDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Create/print delivery challans"
                contentItem: Text { text: parent.text; color: parent.enabled ? "#1b9dd9" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Issue Stock"
                enabled: loginDialog.isAuthenticated
                onClicked: issueDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Issue stock to department"
                contentItem: Text { text: parent.text; color: parent.enabled ? Theme.warning : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Movements"
                onClicked: movementsDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Stock movement audit trail"
                contentItem: Text { text: parent.text; color: "#9b59b6"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Reports"
                onClicked: reportsDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Export reports"
                contentItem: Text { text: parent.text; color: "#16a085"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: {
                    var count = Backend.lowStockCount
                    return "Low Stock" + (count > 0 ? " (" + count + ")" : "")
                }
                onClicked: lowStockDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "View low stock alerts"
                contentItem: Text {
                    text: parent.text
                    color: Backend.lowStockCount > 0 ? Theme.danger : Theme.success
                    horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    font.bold: true; font.pixelSize: 11
                }
            }

            ToolSeparator { contentItem: Rectangle { implicitWidth: 1; implicitHeight: 24; color: Theme.textSecondary } }

            // STOCK OPERATIONS
            ToolButton {
                text: "Search"
                onClicked: searchDialog.open()
                contentItem: Text { text: parent.text; color: Theme.info; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Add Item"
                enabled: loginDialog.isAuthenticated
                onClicked: addItemDialog.open()
                contentItem: Text { text: parent.text; color: parent.enabled ? Theme.info : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            ToolButton {
                text: "+Row"
                enabled: loginDialog.isAuthenticated
                ToolTip.visible: hovered
                ToolTip.text: "Add an empty row, then right-click it to fill it in"
                onClicked: {
                    Backend.model.addRow()
                    rows = Backend.model.rowCount()
                    statusLabel.text = "Row added - right-click it and choose Edit Row to fill it in"
                    statusTimer.restart()
                }
                contentItem: Text { text: parent.text; color: parent.enabled ? "white" : "#7f8c8d"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Issued History"
                onClicked: issueNotesDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "View issue notes history"
                contentItem: Text { text: parent.text; color: Theme.warning; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            Item { Layout.fillWidth: true }

            // LOGIN
            ToolButton {
                text: loginDialog.isAuthenticated ? "Logout" : "Login"
                onClicked: loginDialog.isAuthenticated ? (loginDialog.isAuthenticated = false) : loginDialog.open()
                contentItem: Text { text: parent.text; color: loginDialog.isAuthenticated ? "#2ecc71" : "#e74c3c"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.bold: true; font.pixelSize: 11 }
            }

            // STATUS
            Label {
                text: loginDialog.isAuthenticated ?
                      (Backend.hasUnsavedChanges ? "Unsaved" : "Saved") : "Read-Only"
                color: loginDialog.isAuthenticated ?
                       (Backend.hasUnsavedChanges ? "#e74c3c" : "#2ecc71") : Theme.pending
                font.bold: true; font.pixelSize: 11
            }

            Label { text: Backend.currentUser; color: "white"; font.pixelSize: 11 }

            // DATABASE
            ToolButton {
                text: "Refresh"
                onClicked: {
                    Backend.refreshFromDatabase()
                    rows = Backend.model.rowCount()
                    columns = Backend.model.columnCount()
                    root.tableRefreshToken++
                    refreshStockOverview()
                    statusLabel.text = "Refreshed from database"
                }
                ToolTip.visible: hovered; ToolTip.text: "Reload latest data from the shared database"
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Company"
                onClicked: companyProfileDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Company details printed on purchase orders"
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }

            ToolButton {
                text: "Database"
                onClicked: cloudSettingsDialog.open()
                ToolTip.visible: hovered; ToolTip.text: "Database Connection Settings"
                contentItem: Text { text: parent.text; color: "white"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
            }
        }
    }

    /* ================= MAIN TABLE ================= */

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Working alone when this machine was set up to share. Worth saying
        // loudly and permanently: everything still works, but none of it
        // reaches the people who expect to see it.
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 48
            color: Theme.dangerWash; border.color: Theme.danger; border.width: 1
            visible: root.dbUsingLocalFallback

            RowLayout {
                anchors.fill: parent; anchors.margins: 8; spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true; spacing: 1
                    Label {
                        text: "Not connected to the shared server" +
                              (root.dbConfiguredHost !== "" ? " (" + root.dbConfiguredHost + ")" : "") +
                              " - working on this computer's own copy. Your changes will NOT be seen by anyone else."
                        font.pixelSize: 12; color: Theme.dangerStrong; font.bold: true
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                    Label {
                        text: root.dbFallbackReason
                        visible: text !== ""
                        font.pixelSize: 10; color: "#96504a"
                        Layout.fillWidth: true; elide: Text.ElideRight
                    }
                }

                Button {
                    text: "Retry"
                    ToolTip.visible: hovered
                    ToolTip.text: "Try the shared server again"
                    onClicked: retryDatabaseConnection()
                    contentItem: Text { text: "Retry"; color: Theme.dangerStrong; font.bold: true; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    text: "Settings"
                    onClicked: cloudSettingsDialog.open()
                    contentItem: Text { text: "Settings"; color: Theme.dangerStrong; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
        }

        // Low stock warning
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 35
            color: "#fde8e8"; border.color: Theme.danger; border.width: 1
            visible: Backend.lowStockCount > 0

            RowLayout {
                anchors.fill: parent; anchors.margins: 8; spacing: 10
                Label { text: Backend.lowStockCount + " item(s) below reorder level!"; font.pixelSize: 13; color: Theme.dangerStrong; font.bold: true; Layout.fillWidth: true }
                Button {
                    text: "View"; flat: true
                    onClicked: lowStockDialog.open()
                    contentItem: Text { text: "View Details"; color: Theme.danger; font.bold: true; font.pixelSize: 12 }
                }
            }
        }

        // ---- Headline totals ----
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 78
            color: "#ffffff"; border.color: "#e3e7ea"

            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 28

                Repeater {
                    model: [
                        { label: "Parts tracked", value: (root.stockTotalsData.parts || 0).toString() },
                        { label: "Units in stock", value: Format.grouped(root.stockTotalsData.units || 0).replace(".00", "") },
                        { label: "Stock value", value: Format.rupees(root.stockTotalsData.value || 0) },
                        { label: "Departments", value: (root.stockTotalsData.departments || 0).toString() }
                    ]
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: modelData.label.toUpperCase()
                            font.pixelSize: 10; font.letterSpacing: 0.6
                            color: "#8a9199"
                        }
                        Label {
                            text: modelData.value
                            font.pixelSize: 22; font.bold: true
                            color: "#1f2933"
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                Button {
                    text: root.overviewExpanded ? "Hide breakdown" : "Show breakdown"
                    flat: true
                    onClicked: root.overviewExpanded = !root.overviewExpanded
                    contentItem: Text {
                        text: parent.text; color: "#2a78d6"; font.pixelSize: 12; font.bold: true
                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        // ---- Department segregation + distribution ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(340, Math.max(210,
                                    104 + root.deptSummary.length * 32))
            visible: root.overviewExpanded
            color: "#ffffff"; border.color: "#e3e7ea"

            RowLayout {
                anchors.fill: parent; anchors.margins: 14; spacing: 20

                // Department list: the selector, and the exact numbers behind
                // the ring beside it.
                ColumnLayout {
                    Layout.fillWidth: true; Layout.fillHeight: true; spacing: 6

                    RowLayout {
                        Layout.fillWidth: true
                        Label { text: "STOCK BY DEPARTMENT"; font.pixelSize: 10; font.letterSpacing: 0.6; color: "#8a9199" }
                        Item { Layout.fillWidth: true }
                        Label {
                            text: root.selectedDepartment === ""
                                  ? "showing all"
                                  : "showing " + root.selectedDepartment
                            font.pixelSize: 10; color: "#8a9199"
                        }
                    }

                    // "All" row, then one row per department.
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 30
                        radius: 4
                        color: root.selectedDepartment === "" ? "#eaf2fd" : (allDeptHover.hovered ? "#f4f6f8" : "transparent")
                        border.color: root.selectedDepartment === "" ? "#2a78d6" : "transparent"

                        HoverHandler { id: allDeptHover }
                        TapHandler { onTapped: root.selectedDepartment = "" }

                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 10
                            Rectangle { width: 10; height: 10; radius: 2; color: "#5b6770" }
                            Label { text: "All departments"; font.pixelSize: 12; font.bold: true; color: "#1f2933"; Layout.fillWidth: true }
                            Label {
                                text: (root.stockTotalsData.parts || 0) +
                                      ((root.stockTotalsData.parts === 1) ? " part" : " parts")
                                font.pixelSize: 11; color: "#8a9199"
                            }
                            Label {
                                text: Format.grouped(root.stockTotalsData.units || 0).replace(".00", "")
                                font.pixelSize: 12; font.bold: true; color: "#1f2933"
                                Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight
                            }
                            Item { Layout.preferredWidth: 106 }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true; Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: deptListView
                            model: root.deptSummary
                            spacing: 2
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: Rectangle {
                                width: deptListView.width
                                height: 30
                                radius: 4
                                property bool isSelected: root.selectedDepartment === modelData.department
                                color: isSelected ? "#eaf2fd" : (deptHover.hovered ? "#f4f6f8" : "transparent")
                                border.color: isSelected ? "#2a78d6" : "transparent"

                                HoverHandler { id: deptHover }
                                TapHandler {
                                    // Clicking the selected department again clears the filter.
                                    onTapped: root.selectedDepartment =
                                              isSelected ? "" : modelData.department
                                }

                                RowLayout {
                                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 10

                                    Rectangle {
                                        width: 10; height: 10; radius: 2
                                        color: root.departmentColor(modelData.colorIndex)
                                    }
                                    Label {
                                        text: modelData.department
                                        font.pixelSize: 12; color: "#1f2933"
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Label {
                                        text: modelData.parts + (modelData.parts === 1 ? " part" : " parts")
                                        font.pixelSize: 11; color: "#8a9199"
                                    }
                                    Label {
                                        text: Format.grouped(modelData.qty).replace(".00", "")
                                        font.pixelSize: 12; font.bold: true; color: "#1f2933"
                                        Layout.preferredWidth: 70; horizontalAlignment: Text.AlignRight
                                    }
                                    // Share of total units, on a single-hue track.
                                    Rectangle {
                                        Layout.preferredWidth: 64; Layout.preferredHeight: 6
                                        radius: 3; color: "#eceff1"
                                        Rectangle {
                                            width: Math.max(2, parent.width * modelData.share)
                                            height: parent.height; radius: 3
                                            color: root.departmentColor(modelData.colorIndex)
                                        }
                                    }
                                    Label {
                                        text: Math.round(modelData.share * 100) + "%"
                                        font.pixelSize: 11; color: "#5b6770"
                                        Layout.preferredWidth: 34; horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        text: "No stock rows yet"
                        visible: root.deptSummary.length === 0
                        color: "#8a9199"; font.pixelSize: 11
                    }
                }

                Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: "#e3e7ea" }

                // Distribution ring. Click a slice to filter, hover for the number.
                Item {
                    Layout.preferredWidth: 260; Layout.fillHeight: true

                    Label {
                        id: donutTitle
                        anchors.top: parent.top; anchors.left: parent.left
                        text: "SHARE OF UNITS"
                        font.pixelSize: 10; font.letterSpacing: 0.6; color: "#8a9199"
                    }

                    Canvas {
                        id: donutCanvas
                        anchors.top: donutTitle.bottom; anchors.topMargin: 6
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width, parent.height - 26)
                        height: width

                        property var slices: root.donutSlices()
                        property int hovered: -1
                        onSlicesChanged: requestPaint()
                        onHoveredChanged: requestPaint()

                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            var cx = width / 2, cy = height / 2
                            var outer = Math.min(cx, cy) - 2
                            var inner = outer * 0.62
                            var mid = (outer + inner) / 2

                            var total = 0
                            for (var i = 0; i < slices.length; i++) total += slices[i].qty
                            if (total <= 0) {
                                ctx.strokeStyle = "#eceff1"; ctx.lineWidth = outer - inner
                                ctx.beginPath(); ctx.arc(cx, cy, mid, 0, Math.PI * 2); ctx.stroke()
                                return
                            }

                            // 2px of surface between neighbouring segments, so the
                            // ring reads as separate holdings rather than a blend.
                            var gap = 2 / mid
                            var angle = -Math.PI / 2
                            for (var s = 0; s < slices.length; s++) {
                                var sweep = (slices[s].qty / total) * Math.PI * 2
                                var grow = (hovered === s) ? 3 : 0
                                ctx.beginPath()
                                ctx.strokeStyle = slices[s].color
                                ctx.lineWidth = (outer - inner) + grow
                                var a0 = angle + gap / 2
                                var a1 = angle + sweep - gap / 2
                                if (a1 > a0) {
                                    ctx.arc(cx, cy, mid, a0, a1)
                                    ctx.stroke()
                                }
                                angle += sweep
                            }

                            // Direct-label only the slices with room for it, so
                            // the ring is readable without chasing the legend.
                            ctx.font = "bold 11px sans-serif"
                            ctx.textAlign = "center"
                            ctx.textBaseline = "middle"
                            angle = -Math.PI / 2
                            for (var t = 0; t < slices.length; t++) {
                                var frac = slices[t].qty / total
                                var span = frac * Math.PI * 2
                                if (frac >= 0.10) {
                                    var a = angle + span / 2
                                    ctx.fillStyle = "#ffffff"
                                    ctx.fillText(Math.round(frac * 100) + "%",
                                                 cx + Math.cos(a) * mid,
                                                 cy + Math.sin(a) * mid)
                                }
                                angle += span
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onExited: donutCanvas.hovered = -1
                            onPositionChanged: function(mouse) {
                                var cx = width / 2, cy = height / 2
                                var dx = mouse.x - cx, dy = mouse.y - cy
                                var dist = Math.sqrt(dx * dx + dy * dy)
                                var outer = Math.min(cx, cy) - 2
                                if (dist > outer || dist < outer * 0.62) {
                                    donutCanvas.hovered = -1
                                    return
                                }
                                // Angles run clockwise from twelve o'clock.
                                var a = Math.atan2(dy, dx) + Math.PI / 2
                                if (a < 0) a += Math.PI * 2
                                var total = 0
                                for (var i = 0; i < donutCanvas.slices.length; i++)
                                    total += donutCanvas.slices[i].qty
                                var acc = 0
                                for (var s = 0; s < donutCanvas.slices.length; s++) {
                                    acc += (donutCanvas.slices[s].qty / total) * Math.PI * 2
                                    if (a <= acc) { donutCanvas.hovered = s; return }
                                }
                                donutCanvas.hovered = -1
                            }
                            onClicked: {
                                var h = donutCanvas.hovered
                                if (h < 0) return
                                var slice = donutCanvas.slices[h]
                                if (slice.isOther) return       // a group, not a department
                                root.selectedDepartment =
                                    (root.selectedDepartment === slice.label) ? "" : slice.label
                            }
                        }

                        // Centre reads the total, or the hovered slice.
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 0
                            width: donutCanvas.width * 0.5
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: {
                                    var h = donutCanvas.hovered
                                    if (h >= 0) return Format.grouped(donutCanvas.slices[h].qty).replace(".00", "")
                                    return Format.grouped(root.stockTotalsData.units || 0).replace(".00", "")
                                }
                                font.pixelSize: 20; font.bold: true; color: "#1f2933"
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignHCenter
                                text: {
                                    var h = donutCanvas.hovered
                                    if (h >= 0) return donutCanvas.slices[h].label
                                    return "units total"
                                }
                                font.pixelSize: 10; color: "#8a9199"
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }
                    }
                }
            }
        }

        // Column Headers (9 columns)
        Rectangle {
            id: tableHeader
            Layout.fillWidth: true; Layout.preferredHeight: 35; color: Theme.textPrimary

            Row {
                anchors.fill: parent; spacing: 0

                Rectangle {
                    width: 40; height: 35; color: Theme.textPrimary; border.color: "#1a252f"
                    Text { anchors.centerIn: parent; text: "#"; color: "white"; font.bold: true; font.pixelSize: 11 }
                }

                Repeater {
                    model: 9

                    Rectangle {
                        width: root.columnWidth(index)
                        height: 35; color: "#34495e"; border.color: Theme.textPrimary

                        Text {
                            anchors.centerIn: parent
                            text: {
                                var headers = ["Part Name", "Part No",
                                    root.fileType === "purchase" ? "Purchase" : "Stock",
                                    "Department", "Prepared", "Approved", "Vendor",
                                    "Date", "Unit Price"]
                                return headers[index]
                            }
                            color: "white"; font.bold: true; font.pixelSize: 11
                        }
                    }
                }
            }
        }

        // Data Area
        ScrollView {
            id: stockScroll
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true

            Label {
                anchors.centerIn: parent
                visible: root.visibleStockRows.length === 0
                text: root.selectedDepartment === ""
                      ? "No stock rows"
                      : "No stock rows in " + root.selectedDepartment
                color: "#8a9199"
            }

            ListView {
                id: tableListView
                anchors.fill: parent
                clip: true
                // Only the rows of the chosen department; r stays the real model
                // row so editing, selection and delete keep working unchanged.
                model: root.visibleStockRows.length

                delegate: Row {
                    spacing: 0
                    property int r: root.visibleStockRows[index]

                    Rectangle {
                        width: 40; height: 32
                        color: root.selectedRow === r ? Theme.success : Theme.textMuted
                        border.color: Theme.textSecondary
                        Text { anchors.centerIn: parent; text: r; color: root.selectedRow === r ? "white" : "#2c3e50"; font.bold: root.selectedRow === r; font.pixelSize: 11 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    if (r <= 0) return
                                    root.selectedRow = r
                                    root.contextRow = r
                                    var p = mapToItem(null, mouse.x, mouse.y)
                                    rowContextMenu.popup(p.x, p.y)
                                    return
                                }
                                root.selectedRow = r
                            }
                        }
                    }

                    Repeater {
                        model: 9

                        Rectangle {
                            width: root.columnWidth(index)
                            height: 32
                            color: {
                                var _refresh = root.tableRefreshToken
                                if (root.selectedRow === r) return "#d5f4e6"
                                return "white"
                            }
                            border.color: "#dfe6e9"

                            property int c: index

                            // Read-only on the face of it. Stock is shared by
                            // every machine, so it is changed through the
                            // dialogs that record who did what - never by
                            // typing over a cell in passing.
                            Text {
                                anchors.fill: parent; anchors.margins: 3
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                                text: {
                                    var _refresh = root.tableRefreshToken
                                    var v = Backend.model.getData(r, c)
                                    return v === null || v === undefined ? "" : v.toString()
                                }
                                color: Theme.textPrimary
                                font.pixelSize: 12
                            }

                            // Selecting and the row menu work anywhere on the
                            // row, not just on its number.
                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                onClicked: function(mouse) {
                                    root.selectedRow = r
                                    if (mouse.button === Qt.RightButton && r > 0) {
                                        root.contextRow = r
                                        var p = mapToItem(null, mouse.x, mouse.y)
                                        rowContextMenu.popup(p.x, p.y)
                                    }
                                }
                                onDoubleClicked: openStockRowEditor(r)
                            }
                        }
                    }
                }
            }
        }
    }

    /* ================= FOOTER ================= */

    Menu {
        id: rowContextMenu
        MenuItem {
            text: "Edit Row..."
            enabled: loginDialog.isAuthenticated && root.contextRow > 0
            onTriggered: openStockRowEditor(root.contextRow)
        }
        MenuItem {
            text: "Delete Row"
            enabled: loginDialog.isAuthenticated && root.contextRow > 0
            onTriggered: {
                if (Backend.model.removeRowAt(root.contextRow)) {
                    rows = Backend.model.rowCount()
                    if (root.selectedRow === root.contextRow) root.selectedRow = -1
                }
            }
        }
    }

    footer: ToolBar {
            height: 35
        background: Rectangle { color: "#ecf0f1"; border.color: Theme.divider }

        RowLayout {
            anchors.fill: parent; anchors.margins: 5; spacing: 15

            Label { id: statusLabel; text: "Ready"; font.pixelSize: 12; color: Theme.textPrimary }
            Timer { id: statusTimer; interval: 5000; onTriggered: statusLabel.text = "Ready" }

            Rectangle { width: 1; height: 20; color: Theme.divider }
            Label { text: "Rows: " + rows; font.bold: true; color: "#34495e" }

            Rectangle { width: 1; height: 20; color: Theme.divider }
            Label { text: "Vendors: " + Backend.totalVendors; color: "#1abc9c"; font.bold: true }

            Rectangle { width: 1; height: 20; color: Theme.divider }
            Label { text: "Pending POs: " + Backend.pendingPOCount; color: Backend.pendingPOCount > 0 ? Theme.warning : "#27ae60"; font.bold: true }

            Item { Layout.fillWidth: true }

            Label {
                text: Backend.permanentFile !== "" ? Backend.getFileName() : "No permanent file"
                color: Backend.permanentFile !== "" ? "#27ae60" : Theme.warning
                font.bold: Backend.permanentFile !== ""
            }
        }
    }

    /* ================= INITIALIZATION ================= */






    Component.onCompleted: {
        console.log("========================================")
        console.log("Enstein Stock Manager + Supply Chain Init")

        // Stock is loaded from the shared database by the backend at startup.
        rows = Backend.model.rowCount()
        columns = Backend.model.columnCount()
        root.fileType = "stock"
        refreshDbConnectionState()
        statusLabel.text = root.dbUsingLocalFallback
                ? "Working on this computer's local copy - the shared server did not answer"
                : (Backend.isDatabaseConnected()
                   ? "Ready - stock loaded from database"
                   : "Warning: database not connected")

        refreshStockOverview()

        statusTimer.restart()
        console.log("System initialized")
        console.log("========================================")
    }
}
