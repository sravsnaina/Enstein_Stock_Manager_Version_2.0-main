import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  PrDetailsDialog - a purchase request, read only
// ===========================================================================
//  Includes the review decision and note, so it is clear who approved or
//  rejected a request and why.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var request
    property var lineItems

    title: "Purchase Request " + (request.prNo || "")
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    width: Math.min(App.windowWidth - 120, 720)

    ColumnLayout {
        anchors.fill: parent; spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 4; rowSpacing: 4; columnSpacing: 10

            Label { text: "Request No:"; font.bold: true }
            Label { text: request.prNo || "-"; Layout.fillWidth: true }
            Label { text: "Status:"; font.bold: true }
            Label { text: request.status || "-"; Layout.fillWidth: true }

            Label { text: "Requested by:"; font.bold: true }
            Label { text: request.requestedBy || "-"; Layout.fillWidth: true }
            Label { text: "Department:"; font.bold: true }
            Label { text: request.department || "-"; Layout.fillWidth: true }

            Label { text: "Raised:"; font.bold: true }
            Label { text: request.date || "-"; Layout.fillWidth: true }
            Label { text: "Needed by:"; font.bold: true }
            Label { text: request.neededBy || "-"; Layout.fillWidth: true }

            Label { text: "Priority:"; font.bold: true }
            Label { text: request.priority || "Normal"; Layout.fillWidth: true }
            Label { text: "Ordered as:"; font.bold: true }
            Label { text: request.poNo || "-"; Layout.fillWidth: true }

            Label { text: "Reason:"; font.bold: true; Layout.alignment: Qt.AlignTop }
            Label { text: request.remarks || "-"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
            Label { text: "Reviewed by:"; font.bold: true; Layout.alignment: Qt.AlignTop }
            Label {
                text: (request.reviewedBy || "-") +
                      ((request.reviewNote || "") !== ""
                       ? "\n“" + request.reviewNote + "”" : "")
                Layout.fillWidth: true; wrapMode: Text.WordWrap
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

        Label {
            text: "Items requested (" + lineItems.length + ")"
            font.bold: true; color: Theme.textPrimary
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, 8 + lineItems.length * 26)
            border.color: Theme.border; radius: 4

            ListView {
                id: prDetailItemsView
                anchors.fill: parent; anchors.margins: 4; clip: true
                model: lineItems
                delegate: RowLayout {
                    width: prDetailItemsView.width - 8
                    height: 26
                    spacing: 10
                    Label { text: (index + 1) + ". " + modelData.itemName; font.pixelSize: 11; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: modelData.vendor; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 150; elide: Text.ElideRight }
                    Label { text: modelData.qty + " " + modelData.unit; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 90; horizontalAlignment: Text.AlignRight }
                    Label { text: Format.rupees(modelData.qty * modelData.estimatedPrice); font.pixelSize: 11; color: Theme.warning; Layout.preferredWidth: 140; horizontalAlignment: Text.AlignRight }
                }
            }
        }
    }
}
