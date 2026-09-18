import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  StockSearchDialog - find a part in the stock grid
// ===========================================================================
//  Searches every column and jumps the grid to the row that was picked, rather
//  than filtering it, so the part stays in the context of its neighbours.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: the grid belongs to Main.qml, not here.
    signal rowPicked(int row)

    title: "Search Parts"
    modal: true
    standardButtons: Dialog.Close
    anchors.centerIn: parent
    width: 600; height: 500

    function runSearch() {
        var query = searchField.text.trim()
        searchResultsModel.clear()
        if (query === "") {
            statusLabel.text = "Enter search text"
            return
        }
        var results = Backend.searchAllMatches(query)
        for (var i = 0; i < results.length; i++) searchResultsModel.append(results[i])
        statusLabel.text = results.length === 0 ? "No results found" : "Found " + results.length + " result(s)"
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 10

        Label { text: "Search by Part Name, Part No, or Vendor:"; font.bold: true }

        RowLayout {
            Layout.fillWidth: true; spacing: 10
            TextField {
                id: searchField; Layout.fillWidth: true
                placeholderText: "Enter search text..."; selectByMouse: true
                Keys.onReturnPressed: searchButton.clicked()
                onTextChanged: searchDialog.runSearch()
            }
            Button {
                id: searchButton; text: "Search"; highlighted: true
                onClicked: searchDialog.runSearch()
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            border.color: Theme.divider; border.width: 1; color: "#ecf0f1"

            ListView {
                id: searchResultsList; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 5
                model: ListModel { id: searchResultsModel }
                delegate: Rectangle {
                    width: searchResultsList.width - 10; height: 60; color: "white"
                    border.color: Theme.info; border.width: 1; radius: 5
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 8; spacing: 2
                        Label { text: model.partName; font.bold: true; font.pixelSize: 13; color: Theme.textPrimary }
                        RowLayout {
                            Label { text: "Part No: " + model.partNo; font.pixelSize: 11; color: Theme.textSecondary }
                            Label { text: " | Stock: " + model.stock; font.pixelSize: 11; color: Theme.success; font.bold: true }
                            Label { text: " | Vendor: " + model.vendor; font.pixelSize: 11; color: Theme.textSecondary }
                        }
                    }
                    MouseArea {
                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            // Main.qml owns the grid, so it selects the row and
                            // scrolls to it; this dialog only says which one.
                            rowPicked(model.row)
                            close()
                        }
                    }
                }
            }

            Label { anchors.centerIn: parent; text: "No results"; visible: searchResultsModel.count === 0; color: Theme.textMuted }
        }
    }

    onOpened: { searchField.focus = true; searchField.selectAll() }
}
