import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  ItemMasterDialog - the catalogue of everything we buy and stock
// ===========================================================================
//  One row per part: what it is called, its number, department, preferred
//  vendor, price and required quantity.
//
//  Classification decides which tax code is asked for. A tangible good carries
//  an HSN code, an intangible service a SAC code, and the field beside the
//  radio pair swaps to match. Both codes are stored, so reclassifying an item
//  and changing your mind does not discard the number already typed.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: this dialog cannot see its siblings,
    // so anything that involves another screen leaves as a signal.
    signal dropdownsStale()
    signal partNamesStale(var items)
    signal itemActivated(var item)
    signal vendorPickerRequested(var field)
    property var departmentOptions: Departments.options([])

    title: "Item Master Management"
    modal: true
    anchors.centerIn: parent
    // Takes most of the window instead of a fixed 750x650, so the item list
    // below shows many more rows at once and the wider entry form fits its
    // fields three pairs to a row.
    width: Math.min(root.width - 80, 1240)
    height: Math.min(root.height - 80, 860)

    ColumnLayout{
        anchors.fill: parent; spacing: 10

        Label { text: "Item Master"; font.bold: true; font.pixelSize: 16; color: Theme.textPrimary}

        Rectangle {
            Layout.fillWidth: true
            // Height follows the form rather than a fixed 245, so the
            // classification row added below cannot push the Add button
            // past the bottom edge.
            Layout.preferredHeight: itemEntryForm.implicitHeight + 20
            color: Theme.surfaceAlt; border.color: Theme.border; radius: 5

            ColumnLayout {
                id: itemEntryForm
                anchors.fill: parent; anchors.margins: 10
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: itemIdentitySection.implicitHeight + 16
                    color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                    ColumnLayout {
                        id: itemIdentitySection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                        Label { text: "ITEM IDENTIFICATION & OWNERSHIP"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                        GridLayout {
                            Layout.fillWidth: true; columns: 4; rowSpacing: 8; columnSpacing: 10
                            Label { text: "Part Name:" } TextField { id: itemPartNameField; Layout.fillWidth: true; placeholderText: "Part name"; selectByMouse: true }
                            Label { text: "Part Number:" } TextField { id: itemPartNoField; Layout.fillWidth: true; placeholderText: "Unique part number"; selectByMouse: true }
                            Label { text: "Department:" } ComboBox { id: itemDepartmentField; Layout.fillWidth: true; model: departmentOptions; currentIndex: -1 }
                            Label { text: "Preferred Vendor:" }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 6
                                ComboBox { id: itemVendorField; Layout.fillWidth: true; editable: true; model: Backend.getVendorNames() }
                                Button { text: "Search"; ToolTip.visible: hovered; ToolTip.text: "Search vendors"; onClicked: vendorPickerRequested(itemVendorField) }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: itemPlanningSection.implicitHeight + 16
                        color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                        ColumnLayout {
                            id: itemPlanningSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                            Label { text: "PLANNING & PRICING"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                            GridLayout {
                                Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                                Label { text: "Unit Price:" }
                                TextField {
                                    id: itemUnitPriceField
                                    Layout.fillWidth: true
                                    placeholderText: "0.00"
                                    text: "0.00"
                                    validator: DoubleValidator { bottom: 0; decimals: 2 }
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                    selectByMouse: true
                                }
                                Label { text: "Required Quantity:" } SpinBox { id: itemRequiredQtyField; Layout.fillWidth: true; from: 0; to: 100000; value: 0; editable: true }
                                Label { text: "Unit:" } TextField { id: itemUnitField; Layout.fillWidth: true; placeholderText: "Nos, Kg, Mtr"; selectByMouse: true }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: itemClassSection.implicitHeight + 16
                        color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                        ColumnLayout {
                            id: itemClassSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                            Label { text: "TAX CLASSIFICATION"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 14
                                RadioButton { id: itemTangibleRadio; text: "Tangible (HSN)"; checked: true }
                                RadioButton { id: itemIntangibleRadio; text: "Intangible (SAC)" }
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: 10
                                Label { text: itemTangibleRadio.checked ? "HSN Code:" : "SAC Code:" }
                                StackLayout {
                                    Layout.fillWidth: true; currentIndex: itemTangibleRadio.checked ? 0 : 1
                                    TextField { id: itemHsnField; placeholderText: "e.g. 85015210"; selectByMouse: true }
                                    TextField { id: itemSacField; placeholderText: "e.g. 998313"; selectByMouse: true }
                                }
                            }
                        }
                    }
                }

                // Spans the whole row so the button sits at the right edge
                // whether the grid is showing two pairs or three.
                Button {
                    text: "Add/Update Item"; highlighted: true
                    Layout.alignment: Qt.AlignRight
                    onClicked: {
                        var itemMasterDetails = {
                            partName: itemPartNameField.text,
                            partNo: itemPartNoField.text,
                            department: itemDepartmentField.currentText,
                            category: itemDepartmentField.currentText,
                            unitPrice: Format.amount(itemUnitPriceField.text),
                            requiredQty: itemRequiredQtyField.value,
                            stockQty: itemRequiredQtyField.value,
                            vendor: itemVendorField.currentText,
                            itemType: itemTangibleRadio.checked ? "Tangible" : "Intangible",
                            hsnCode: itemHsnField.text,
                            sacCode: itemSacField.text,
                            unit: itemUnitField.text
                        }
                        if (Backend.addItemMasterDetails(itemMasterDetails)) {
                            refresh()
                            itemPartNameField.text = ""; itemPartNoField.text = ""; itemDepartmentField.currentIndex = -1;
                            itemUnitPriceField.text = "0.00"; itemRequiredQtyField.value = 0; itemVendorField.currentIndex = -1;
                            itemHsnField.text = ""; itemSacField.text = ""; itemUnitField.text = "";
                            itemTangibleRadio.checked = true;
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Label { text: "Search:"; font.bold: true }
            TextField {
                id: itemSearchField
                Layout.fillWidth: true
                placeholderText: "Search by part name, part no, department, vendor, HSN/SAC..."
                selectByMouse: true
                Keys.onReturnPressed: itemSearchButton.clicked()
                onTextChanged: refresh(text)
            }
            Button {
                id: itemSearchButton
                text: "Search"
                onClicked: refresh(itemSearchField.text)
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            border.color: Theme.border; radius: 5

            ListView {
                id: itemMasterListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                model: ListModel { id: itemMasterListModel }
                delegate: Rectangle {
                    width: itemMasterListView.width - 10; height: 70; color: "#fff"; border.color: Theme.borderSubtle; radius: 4
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 8
                        Item {
                            id: itemInfoArea
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            ColumnLayout {
                                anchors.fill: parent
                                spacing: 2
                                Label {
                                    text: model.partName + " (" + model.partNo + ")"
                                    font.bold: true; font.pixelSize: 13; color: Theme.textPrimary
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Label {
                                    text: "Department: " + model.department + " | Vendor: " + model.vendor
                                    font.pixelSize: 11; color: Theme.textSecondary
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                                Label {
                                    text: "Unit Price: " + Format.rupees(model.unitPrice) + " | Required Qty: " + model.requiredQty +
                                          " | " + model.itemType +
                                          (model.taxCode !== "" ? " | " + (model.itemType === "Intangible" ? "SAC: " : "HSN: ") + model.taxCode : "") +
                                          (model.unit !== "" ? " | Unit: " + model.unit : "")
                                    font.pixelSize: 10; color: Theme.textMuted
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    itemActivated({
                                        partName: model.partName,
                                        partNo: model.partNo,
                                        department: model.department,
                                        unitPrice: model.unitPrice,
                                        requiredQty: model.requiredQty,
                                        vendor: model.vendor,
                                        itemType: model.itemType,
                                        hsnCode: model.hsnCode,
                                        sacCode: model.sacCode,
                                        unit: model.unit
                                    })
                                }
                            }
                        }
                        Button {
                            id: itemDeleteButton
                            Layout.alignment: Qt.AlignRight
                            text: "Delete"; flat: true
                            contentItem: Text { text: "Delete"; color: Theme.danger; font.pixelSize: 11 }
                            onClicked: { Backend.deleteItem(model.partName); refresh() }
                        }
                    }
                }
            }

             Label { anchors.centerIn: parent; text: "No items added"; visible: itemMasterListModel.count === 0; color: Theme.textMuted }
        }

        Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: close() }
    }

    onOpened: {
        refresh()
        departmentOptions = Departments.options(Backend.getItemMasterList())
        dropdownsStale()
    }

    // Called with no argument to re-apply whatever filter is currently
    // typed, which is what the backend's change signals want: refresh the
    // list without throwing away what the user was searching for.
    function refresh(filterText) {
        if (filterText === undefined) filterText = itemSearchField.text
        itemMasterListModel.clear()
        var query = (filterText || "").toString().trim().toLowerCase()
        var items = Backend.getItemMasterList()
        for (var i = 0; i < items.length; i++){
            var item = items[i];
                var department = item.department || item.category || ""
                var requiredQty = item.requiredQty || item.stockQty || 0
                var itemType = item.itemType || "Tangible"
                var haystack = [
                    item.partName, item.partNo, department, item.vendor,
                    itemType, item.hsnCode, item.sacCode
                ].join(" ").toLowerCase()
                if (query !== "" && haystack.indexOf(query) === -1) continue
                itemMasterListModel.append({
                    partName: item.partName || "",
                    partNo: item.partNo || "",
                    department: department,
                    unitPrice: item.unitPrice || 0,
                    requiredQty: requiredQty,
                    vendor: item.vendor || "",
                    itemType: itemType,
                    hsnCode: item.hsnCode || "",
                    sacCode: item.sacCode || "",
                    // Whichever of the two the classification says to print.
                    taxCode: item.taxCode || "",
                    unit: item.unit || "",
              })
        }
        partNamesStale(items)
    }

    function refreshVendorOptions(vendorNames) {
        itemVendorField.model = vendorNames
    }
}
