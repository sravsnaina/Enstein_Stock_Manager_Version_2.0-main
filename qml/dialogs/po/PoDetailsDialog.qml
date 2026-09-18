import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  PoDetailsDialog - a purchase order, read only
// ===========================================================================
//  Everything on the order and its lines. Editing happens in PoEditDialog; this
//  is what you open to answer a question about an order.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var order
    property var lineItems

    title: "PO Details"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok
    width: 680

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label { text: "PO No: " + (order.poNo || "-"); font.bold: true; color: Theme.textPrimary }

        // Line items of this PO (each with its own vendor)
        Label { text: "Items (" + lineItems.length + "):"; font.bold: true }
        Repeater {
            model: lineItems
            Label {
                text: "  " + (index + 1) + ". " + (modelData.partName || "-") +
                      " (" + (modelData.partNo || "-") + ")  x" + (modelData.qty || 0) +
                      " @ " + Format.rupees(modelData.unitPrice || 0) +
                      "  | Vendor: " + (modelData.vendor || "-") +
                      "  | Recv: " + (modelData.receivedQty || 0) + "/" + (modelData.qty || 0)
                font.pixelSize: 12
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        Label { text: "Department: " + (order.department || "-"); visible: lineItems.length <= 1 }
        Label { text: "Vendor(s): " + (order.vendor || "-") }
        Label { text: "Total Qty: " + (order.qty || 0) }
        Label { text: "Total Price: " + Format.rupees(order.totalAmount || 0); font.bold: true; color: Theme.success }
        Label {
            text: "Needed Period: " + Format.period(order.expectedDate,
                                                   order.expectedEndDate)
            font.bold: true
        }
        Label { text: "Status: " + (order.status || "-") }
        Label { text: "Prepared By: " + (order.preparedBy || "-") }
        Label { text: "Approved By: " + (order.approvedBy || "-") }
        Label { text: "Received By: " + (order.receivedBy || "-") }
        Label { text: "Received Date: " + (order.receivedDate || "-") }
        Label { text: "Received Qty: " + (order.receivedQty || 0) + " / " + (order.qty || 0) }
    }
}
