import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  ReportsDialog - export a period to a spreadsheet
// ===========================================================================
//  Writes orders, movements, issues and receipts for a date range into one
//  .xlsx workbook, a sheet each.
// ===========================================================================
Dialog {
    title: "Export Reports"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Close
    width: 420

    ColumnLayout {
        anchors.fill: parent; spacing: 10
        Label { text: "Export stock movements, issues, GRN, and POs"; font.bold: true }

        Label { text: "From Date (YYYY-MM-DD):" }
        TextField { id: reportFromField; Layout.fillWidth: true; placeholderText: "2025-01-01" }

        Label { text: "To Date (YYYY-MM-DD):" }
        TextField { id: reportToField; Layout.fillWidth: true; placeholderText: "2025-01-31" }

        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "Export"
                highlighted: true
                onClicked: {
                    var path = Backend.browseSaveFile("Save Report", "Excel files (*.xlsx)")
                    if (path === "") return
                    if (Backend.exportReport(reportFromField.text, reportToField.text, path)) {
                        statusLabel.text = "Report saved"
                        statusTimer.restart()
                    }
                }
            }
            Item { Layout.fillWidth: true }
        }
    }
}
