import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  ErrorDialog - one place every failure is shown
// ===========================================================================
//  The backend reports problems by emitting errorOccurred rather than showing
//  anything itself, and Main.qml routes all of them here. Set errorText, then
//  open().
// ===========================================================================
Dialog {
    title: "Error"
    modal: true
    standardButtons: Dialog.Ok
    anchors.centerIn: parent
    width: 400
    property alias errorText: errorLabel.text
    Label { id: errorLabel; wrapMode: Text.WordWrap; width: parent.width }
}
