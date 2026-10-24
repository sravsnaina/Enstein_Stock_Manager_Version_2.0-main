import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  DcSavedDialog - confirms where a delivery challan PDF was written
// ===========================================================================
//  The challan equivalent of PoSavedDialog.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var pdfPath

    title: "Delivery Challan Saved"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok
    width: parent ? Math.min(560, parent.width - 32) : 560
    height: Math.max(1, Math.min(implicitHeight, App.windowHeight - 32))

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "The delivery challan PDF has been saved to:"; font.bold: true }
        Label {
            text: pdfPath
            wrapMode: Text.WrapAnywhere
            Layout.fillWidth: true
            color: Theme.textPrimary
        }
        Button {
            text: "Open the file"
            onClicked: Backend.openInSystemViewer(pdfPath)
        }
    }
}
