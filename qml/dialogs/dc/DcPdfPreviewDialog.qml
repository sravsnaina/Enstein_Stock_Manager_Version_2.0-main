import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  DcPdfPreviewDialog - see the challan before it travels with the goods
// ===========================================================================
//  The challan equivalent of PoPdfPreviewDialog.
// ===========================================================================
Dialog {
    // Supplied by Main.qml; this dialog only reads them.
    property var dcNo
    property var pages
    property var pdfPath

    // Raised for Main.qml to act on: this dialog does not know
    // what else has to happen afterwards.
    signal sendRequested()

    title: "Delivery Challan " + dcNo
    modal: true
    anchors.centerIn: parent
    width: Math.min(App.windowWidth - 80, 940)
    height: Math.min(App.windowHeight - 60, 900)
    standardButtons: Dialog.NoButton

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#525659"
            radius: 4

            ListView {
                id: dcPreviewView
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 12
                model: pages
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Rectangle {
                    width: dcPreviewView.width - 20
                    // A4 aspect ratio, so the page keeps its proportions
                    // whatever the dialog is resized to.
                    height: width * 297 / 210
                    color: "white"
                    border.color: "#2c2c2c"

                    Image {
                        anchors.fill: parent
                        source: modelData
                        fillMode: Image.PreserveAspectFit
                        smooth: true
                        asynchronous: true
                        cache: false
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: pages.length + (pages.length === 1 ? " page" : " pages")
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "Open in PDF viewer"
                onClicked: Backend.openInSystemViewer(pdfPath)
            }
            Button {
                text: "Close"
                onClicked: dcPdfPreviewDialog.close()
            }
            Button {
                text: "Save PDF"
                highlighted: true
                ToolTip.visible: hovered
                ToolTip.text: "Save this delivery challan as a PDF on this computer"
                onClicked: sendRequested()
            }
        }
    }
}
