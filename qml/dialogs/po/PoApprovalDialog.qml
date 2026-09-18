import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  PoApprovalDialog - send an order for approval
// ===========================================================================
//  Records who approved it, which is printed on the order and is the reason a
//  name is asked for rather than assumed.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var poNo

    // Raised for Main.qml to act on: this dialog does not know
    // what else has to happen afterwards.
    signal orderChanged()

    title: "Approve PO"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 380

    ColumnLayout {
        spacing: 10
        Label { text: "PO: " + poNo; font.bold: true }
        Label { text: "Approved By*:" }
        TextField {
            id: approvedByField
            Layout.fillWidth: true
            placeholderText: "Enter approver name"
            selectByMouse: true
        }
    }

    onAccepted: {
        if (Backend.sendPOForApproval(poNo, approvedByField.text)) {
            orderChanged()
        }
    }

    function showForApproval(number) {
        poNo = number
        approvedByField.text = ""
        open()
        approvedByField.forceActiveFocus()
    }
}
