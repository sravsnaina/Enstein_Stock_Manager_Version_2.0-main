import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  ItemDetailsDialog - edit one item master row
// ===========================================================================
//  Part numbers are unique and are what the backend matches on, so the number the
//  item had when the dialog opened is kept in originalPartNo.
//
//  Classification decides which tax code applies: a tangible good carries an HSN
//  code, an intangible service a SAC code. Both are kept, so switching an item
//  over and back does not lose the number already typed.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: this dialog cannot see its
    // siblings, so anything involving another screen goes out as a signal.
    signal itemChanged()
    signal vendorPickerRequested(var field)
    property var departmentOptions: Departments.options([])

    title: "Item Details"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok | Dialog.Cancel
    // Wide enough for three field pairs per row, and never taller than the
    // window: the fields scroll inside rather than pushing the OK and
    // Cancel buttons off the bottom edge.
    width: Math.min(App.windowWidth - 120, 1100)
    height: Math.min(App.windowHeight - 120, 520)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "Edit Item Details"; font.bold: true; font.pixelSize: 14 }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

        ScrollView {
            id: editItemScroll
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            GridLayout {
                width: editItemScroll.availableWidth
                columns: width > 900 ? 6 : 4
                rowSpacing: 8; columnSpacing: 10

                Label { text: "Part Name:" } TextField { id: editItemPartNameField; Layout.fillWidth: true; Layout.preferredWidth: 150; placeholderText: "Part name" }
                Label { text: "Part No:" } TextField { id: editItemPartNoField; Layout.fillWidth: true; Layout.preferredWidth: 150; placeholderText: "Part number" }
                Label { text: "Department:" }
                ComboBox { id: editItemDepartmentField; Layout.fillWidth: true; Layout.preferredWidth: 150; model: departmentOptions; currentIndex: -1 }
                Label { text: "Unit Price:" }
                TextField {
                    id: editItemUnitPriceField
                    Layout.fillWidth: true
                    Layout.preferredWidth: 150
                    placeholderText: "0.00"
                    text: "0.00"
                    validator: DoubleValidator { bottom: 0; decimals: 2 }
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    selectByMouse: true
                }
                Label { text: "Required Quantity:" } SpinBox { id: editItemRequiredQtyField; Layout.fillWidth: true; Layout.preferredWidth: 150; from: 0; to: 100000; value: 0; editable: true }
                Label { text: "Vendor Preferred:" }
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredWidth: 150; spacing: 6
                    ComboBox { id: editItemVendorField; Layout.fillWidth: true; editable: true; model: Backend.getVendorNames() }
                    Button {
                        text: "\u{1F50D}"
                        Layout.preferredWidth: 40
                        ToolTip.visible: hovered
                        ToolTip.text: "Search vendors"
                        onClicked: vendorPickerRequested(editItemVendorField)
                    }
                }

                // Same pairing as the add form: the classification decides
                // whether this item is numbered by HSN or by SAC.
                Label { text: "Classification:" }
                RowLayout {
                    Layout.fillWidth: true; Layout.preferredWidth: 150; spacing: 14
                    RadioButton {
                        id: editItemTangibleRadio
                        text: "Tangible"; checked: true
                        ToolTip.visible: hovered
                        ToolTip.text: "A physical good, numbered with an HSN code"
                    }
                    RadioButton {
                        id: editItemIntangibleRadio
                        text: "Intangible"
                        ToolTip.visible: hovered
                        ToolTip.text: "A service, numbered with a SAC code"
                    }
                }

                Label { text: editItemTangibleRadio.checked ? "HSN Code:" : "SAC Code:" }
                StackLayout {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 150
                    currentIndex: editItemTangibleRadio.checked ? 0 : 1
                    TextField { id: editItemHsnField; placeholderText: "e.g. 85015210"; selectByMouse: true }
                    TextField { id: editItemSacField; placeholderText: "e.g. 998313"; selectByMouse: true }
                }

                Label { text: "Unit:" } TextField { id: editItemUnitField; Layout.fillWidth: true; Layout.preferredWidth: 150; placeholderText: "Nos, Kg, Mtr..."; selectByMouse: true }
            }
        }
    }

    onAccepted: {
        var itemDetails = {
            originalPartNo: originalPartNo,
            partName: editItemPartNameField.text,
            partNo: editItemPartNoField.text,
            department: editItemDepartmentField.currentText,
            category: editItemDepartmentField.currentText,
            unitPrice: Format.amount(editItemUnitPriceField.text),
            requiredQty: editItemRequiredQtyField.value,
            stockQty: editItemRequiredQtyField.value,
            vendor: editItemVendorField.currentText,
            itemType: editItemTangibleRadio.checked ? "Tangible" : "Intangible",
            hsnCode: editItemHsnField.text,
            sacCode: editItemSacField.text,
            unit: editItemUnitField.text
        }

        if (Backend.updateItemMasterDetails(itemDetails)) {
            itemChanged()
        }
    }

    function showItem(item) {
        originalPartNo = item.partNo || ""
        editItemPartNameField.text = item.partName || ""
        editItemPartNoField.text = item.partNo || ""
        departmentOptions = Departments.options(Backend.getItemMasterList())
        editItemDepartmentField.currentIndex = Departments.indexOf(departmentOptions, item.department || "")
        editItemUnitPriceField.text = Format.fixed(item.unitPrice)
        editItemRequiredQtyField.value = item.requiredQty || 0
        editItemVendorField.editText = item.vendor || ""
        editItemVendorField.currentIndex = -1
        // Setting the losing radio too: leaving it checked from the previous
        // item would show two selected buttons in the same group.
        var intangible = (item.itemType || "") === "Intangible"
        editItemIntangibleRadio.checked = intangible
        editItemTangibleRadio.checked = !intangible
        editItemHsnField.text = item.hsnCode || ""
        editItemSacField.text = item.sacCode || ""
        editItemUnitField.text = item.unit || ""
        open()
    }

    function refreshVendorOptions(vendorNames) {
        editItemVendorField.model = vendorNames
    }
    property string originalPartNo: ""
}
