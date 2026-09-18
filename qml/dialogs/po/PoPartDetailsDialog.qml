import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  PoPartDetailsDialog - what the item master knows about a part
// ===========================================================================
//  Opened from a purchase order line so the person raising the order can check
//  the price and preferred vendor without leaving the form.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var part

    title: "Item Master Details"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok
    width: 430

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        Label { text: "Part Name: " + (part.partName || "-"); font.bold: true; color: Theme.textPrimary }
        Label { text: "Part No: " + (part.partNo || "-") }
        Label { text: "Department: " + (part.department || "-") }
        Label { text: "Required Quantity: " + (part.requiredQty || 0) }
        Label { text: "Unit Price: " + Format.rupees(part.unitPrice || 0); color: Theme.success; font.bold: true }
        Label { text: "Preferred Vendor: " + (part.vendor || "-") }
    }
}
