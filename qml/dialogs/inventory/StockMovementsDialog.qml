import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  StockMovementsDialog - the audit trail
// ===========================================================================
//  Every quantity change, newest first: what moved, which way, how much and what
//  caused it. This is the screen to open when the shelf and the screen disagree.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: this dialog does not know
    // what else has to happen afterwards.
    signal refreshRequested()

    title: "Stock Movement History"
    modal: true
    anchors.centerIn: parent
    width: 800; height: 600

    ColumnLayout {
        anchors.fill: parent; spacing: 10

        Label { text: "Audit Trail - All Stock Movements"; font.bold: true; font.pixelSize: 16; color: Theme.textPrimary }

        RowLayout {
            Layout.fillWidth: true; spacing: 10
            TextField {
                id: movFilterField; Layout.fillWidth: true
                placeholderText: "Filter by part name..."; selectByMouse: true
                onTextChanged: refreshRequested()
            }
            Label { text: movementsModel.count + " records"; font.bold: true; color: Theme.textSecondary }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            border.color: Theme.border; radius: 5

            // Header
            Rectangle {
                id: movHeader; width: parent.width; height: 30; color: Theme.textPrimary
                Row {
                    anchors.fill: parent; spacing: 0
                    Repeater {
                        model: [
                            {text: "Date", w: 150}, {text: "Part Name", w: 180},
                            {text: "Type", w: 80}, {text: "Qty", w: 60},
                            {text: "Reference", w: 200}, {text: "Done By", w: 110}
                        ]
                        Rectangle {
                            width: modelData.w; height: 30; color: "transparent"
                            Text { anchors.centerIn: parent; text: modelData.text; color: "white"; font.bold: true; font.pixelSize: 11 }
                        }
                    }
                }
            }

            ListView {
                id: movListView
                anchors.top: movHeader.bottom; anchors.left: parent.left
                anchors.right: parent.right; anchors.bottom: parent.bottom
                anchors.margins: 2; clip: true; spacing: 1
                model: ListModel { id: movementsModel }
                delegate: Rectangle {
                    width: movListView.width; height: 30
                    color: index % 2 === 0 ? "#fff" : Theme.surfaceAlt
                    Row {
                        anchors.fill: parent; spacing: 0
                        Rectangle { width: 150; height: 30; color: "transparent"; Text { anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.date; font.pixelSize: 11 } }
                        Rectangle { width: 180; height: 30; color: "transparent"; Text { anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.partName; font.pixelSize: 11; font.bold: true } }
                        Rectangle {
                            width: 80; height: 30; color: "transparent"
                            Text {
                                anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.type; font.pixelSize: 11; font.bold: true
                                color: {
                                    if (model.type === "IN") return Theme.success
                                    if (model.type === "OUT") return Theme.danger
                                    if (model.type === "REJECTED") return Theme.warning
                                    return Theme.info
                                }
                            }
                        }
                        Rectangle { width: 60; height: 30; color: "transparent"; Text { anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.qty; font.pixelSize: 11; font.bold: true } }
                        Rectangle { width: 200; height: 30; color: "transparent"; Text { anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.reference; font.pixelSize: 11; color: Theme.textSecondary } }
                        Rectangle { width: 110; height: 30; color: "transparent"; Text { anchors.verticalCenter: parent.verticalCenter; x: 5; text: model.doneBy; font.pixelSize: 11 } }
                    }
                }
            }

            Label { anchors.centerIn: parent; y: 30; text: "No movements recorded"; visible: movementsModel.count === 0; color: Theme.textMuted }
        }

        Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: movementsDialog.close() }
    }

    onOpened: { refreshRequested() }
}
