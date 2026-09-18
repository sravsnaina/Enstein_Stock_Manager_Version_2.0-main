import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  PoSavedDialog - confirms where a purchase order PDF was written
// ===========================================================================
//  Shows the path and offers to open it, because a file saved somewhere the
//  user cannot find is barely saved at all.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var pdfPath

    title: "Purchase Order Saved"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok
    width: 560

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "The purchase order PDF has been saved to:"; font.bold: true }
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
