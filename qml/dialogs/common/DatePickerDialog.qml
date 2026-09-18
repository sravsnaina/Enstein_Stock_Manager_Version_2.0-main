import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.Basic 6.3 as BasicControls
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  DatePickerDialog - the shared calendar
// ===========================================================================
//  Every date field in the application picks through this one dialog. The caller
//  sets targetField, purpose and seedText via openDatePicker() in Main.qml;
//  seedText is what makes picking the end of a period start from its beginning
//  rather than from today.
// ===========================================================================
Dialog {
    title: "Select " + purpose
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok | Dialog.Cancel
    width: 420
    height: 420
    property int displayMonth: (new Date()).getMonth() + 1
    property int displayYear: (new Date()).getFullYear()
    property date selectedDate: new Date()
    property var targetField: null
    property string purpose: "Date"
    property string seedText: ""

    onOpened: {
        var parsed = new Date(seedText)
        if (!isNaN(parsed.getTime())) {
            selectedDate = parsed
        } else {
            selectedDate = new Date()
        }
        displayMonth = selectedDate.getMonth() + 1
        displayYear = selectedDate.getFullYear()
    }

    onAccepted: {
        if (targetField) targetField.text = Qt.formatDate(selectedDate, "yyyy-MM-dd")
    }

    ColumnLayout {
        anchors.fill: parent
        RowLayout {
            Layout.fillWidth: true
            Button {
                text: "<"
                onClicked: {
                    expectedDateDialog.displayMonth--
                    if (expectedDateDialog.displayMonth < 1) {
                        expectedDateDialog.displayMonth = 12
                        expectedDateDialog.displayYear--
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: Qt.formatDate(new Date(expectedDateDialog.displayYear,
                                             expectedDateDialog.displayMonth - 1, 1),
                                    "MMMM yyyy")
                font.bold: true
            }
            Button {
                text: ">"
                onClicked: {
                    expectedDateDialog.displayMonth++
                    if (expectedDateDialog.displayMonth > 12) {
                        expectedDateDialog.displayMonth = 1
                        expectedDateDialog.displayYear++
                    }
                }
            }
        }

        BasicControls.DayOfWeekRow {
            locale: Qt.locale()
            Layout.fillWidth: true
        }

        BasicControls.MonthGrid {
            id: expectedDateMonthGrid
            month: expectedDateDialog.displayMonth
            year: expectedDateDialog.displayYear
            locale: Qt.locale()
            Layout.fillWidth: true
            Layout.fillHeight: true

            onClicked: function(date) {
                expectedDateDialog.selectedDate = date
            }

            delegate: Rectangle {
                required property var model
                color: {
                    var selected = Qt.formatDate(expectedDateDialog.selectedDate, "yyyy-MM-dd")
                    var current = Qt.formatDate(model.date, "yyyy-MM-dd")
                    if (selected === current) return Theme.info
                    return "transparent"
                }
                radius: width / 2

                Text {
                    anchors.centerIn: parent
                    text: model.day
                    color: {
                        if (Qt.formatDate(expectedDateDialog.selectedDate, "yyyy-MM-dd") ===
                            Qt.formatDate(model.date, "yyyy-MM-dd")) {
                            return "white"
                        }
                        return model.month === expectedDateMonthGrid.month ? "#2c3e50" : Theme.divider
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: expectedDateDialog.selectedDate = model.date
                }
            }
        }
    }
}
