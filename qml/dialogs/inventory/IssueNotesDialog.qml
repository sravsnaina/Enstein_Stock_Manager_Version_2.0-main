import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  IssueNotesDialog - what has been handed out
// ===========================================================================
//  A read-only history of issue notes: what left the shelf, to which department
//  and who authorised it.
// ===========================================================================
Dialog {
    title: "Issue Notes History"
    modal: true
    anchors.centerIn: parent
    width: 700; height: 500

    ColumnLayout {
        anchors.fill: parent; spacing: 10

        Label { text: "Material Issue History"; font.bold: true; font.pixelSize: 16; color: Theme.warning }

        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            border.color: Theme.border; radius: 5

            ListView {
                id: issueNotesListView; anchors.fill: parent; anchors.margins: 5; clip: true; spacing: 4
                model: ListModel { id: issueNotesModel }
                delegate: Rectangle {
                    width: issueNotesListView.width - 10; height: 50
                    color: "#fff"; border.color: Theme.warning; border.width: 1; radius: 4
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 8
                        Label { text: model.issueNo; font.bold: true; color: Theme.warning; Layout.preferredWidth: 100 }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 2
                            Label { text: model.partName + " x" + model.qty; font.bold: true; font.pixelSize: 12 }
                            Label { text: "To: " + model.department + " | By: " + model.issuedBy + " | " + model.date; font.pixelSize: 10; color: Theme.textSecondary }
                        }
                    }
                }
            }

            Label { anchors.centerIn: parent; text: "No issues recorded"; visible: issueNotesModel.count === 0; color: Theme.textMuted }
        }

        Button { text: "Close"; Layout.alignment: Qt.AlignRight; onClicked: issueNotesDialog.close() }
    }

    onOpened: {
        issueNotesModel.clear()
        var notes = Backend.getIssueNotes()
        for (var i = 0; i < notes.length; i++) issueNotesModel.append(notes[i])
    }
}
