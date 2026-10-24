import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  LowStockDialog - what we are about to run out of
// ===========================================================================
//  An item is low when stock plus what is already on order cannot cover its
//  required quantity. Offers to raise one draft order covering every shortage,
//  grouped by preferred vendor.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: this dialog does not know
    // what else has to happen afterwards.
    signal refreshRequested()
    signal orderChanged()

    title: "Low Stock Alerts"
    modal: true
    anchors.centerIn: parent
    width: parent ? Math.min(700, parent.width - 32) : 700
    height: Math.max(1, Math.min(500, App.windowHeight - 32))

    ColumnLayout {
        anchors.fill: parent; spacing: 10

        Label { text: "Items Below Reorder Level"; font.bold: true; font.pixelSize: 16; color: Theme.danger }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            border.color: Theme.border; radius: 5

            ListView {
                id: lowStockListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                model: ListModel { id: lowStockModel }
                delegate: Rectangle {
                    width: lowStockListView.width - 10; height: 60 * App.dialogScale
                    color: "#fff5f5"; border.color: Theme.danger; border.width: 1; radius: 4
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 8
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Label { text: model.partName + (model.partNo ? "  (" + model.partNo + ")" : ""); font.bold: true; font.pixelSize: 13 }
                            Label {
                                text: "Stock: " + model.stock + " | Required: " + model.requiredQty + " | On order: " + model.onOrder + " | Shortage: " + model.shortage
                                font.pixelSize: 11; color: Theme.danger
                            }
                        }
                        Label { text: model.vendor ? "Vendor: " + model.vendor : "No vendor set!"; font.pixelSize: 11; color: model.vendor ? Theme.textSecondary : Theme.danger }
                        Label { text: "Order qty:"; font.pixelSize: 11; color: Theme.textSecondary }
                        SpinBox {
                            from: 1; to: 100000; editable: true
                            value: model.orderQty
                            onValueModified: lowStockModel.setProperty(index, "orderQty", value)
                        }
                    }
                }
            }

            Label { anchors.centerIn: parent; text: "All stock levels are OK!"; visible: lowStockModel.count === 0; color: Theme.success; font.pixelSize: 14 }
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Auto-Generate PO for Low Stock"
                highlighted: true
                enabled: lowStockModel.count > 0
                onClicked: {
                    var quantities = []
                    for (var i = 0; i < lowStockModel.count; ++i) {
                        var item = lowStockModel.get(i)
                        quantities.push({ partNo: item.partNo, partName: item.partName, qty: item.orderQty })
                    }
                    if (Backend.autoGeneratePOForLowStock(quantities)) {
                        statusLabel.text = "Draft PO generated for low stock items"
                        statusTimer.restart()
                        refreshRequested()
                        orderChanged()
                    }
                }
            }
            Item { Layout.fillWidth: true }
            Button { text: "Close"; onClicked: close() }
        }
    }

    // The list model belongs to this component, so it must be populated here.
    // Main.qml cannot safely refer to the internal lowStockModel ID.
    function refresh() {
        lowStockModel.clear()
        var items = Backend.getLowStockItems()
        for (var i = 0; i < items.length; ++i) {
            var item = items[i]
            item.orderQty = Math.max(1, item.shortage)
            lowStockModel.append(item)
        }
    }

    onOpened: refresh()
}
