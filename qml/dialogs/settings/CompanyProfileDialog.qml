import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  CompanyProfileDialog - our own details
// ===========================================================================
//  Name, address, GSTIN and contact details, printed as the letterhead on every
//  purchase order and delivery challan.
// ===========================================================================
Dialog {
    title: "Company Profile"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Save | Dialog.Cancel
    width: Math.min(App.windowWidth - 120, 760)

    // These details head every printed purchase order, so they are worth
    // filling in once before sending anything to a vendor.
    onOpened: {
        var c = Backend.getCompanyProfile()
        companyNameField.text = c.name || ""
        companyAddr1Field.text = c.addressLine1 || ""
        companyAddr2Field.text = c.addressLine2 || ""
        companyCityField.text = c.city || ""
        companyPhoneField.text = c.phone || ""
        companyEmailField.text = c.email || ""
        companyWebsiteField.text = c.website || ""
        companyGstinField.text = c.gstin || ""
    }

    onAccepted: {
        Backend.saveCompanyProfile({
            name: companyNameField.text,
            addressLine1: companyAddr1Field.text,
            addressLine2: companyAddr2Field.text,
            city: companyCityField.text,
            phone: companyPhoneField.text,
            email: companyEmailField.text,
            website: companyWebsiteField.text,
            gstin: companyGstinField.text
        })
        statusLabel.text = "Company profile saved"
        statusTimer.restart()
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 8

        Rectangle {
            Layout.fillWidth: true; implicitHeight: companyIdentitySection.implicitHeight + 16
            color: Theme.surfaceAlt; border.color: Theme.borderSubtle; radius: 4
            ColumnLayout {
                id: companyIdentitySection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                Label { text: "COMPANY IDENTITY"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                GridLayout {
                    Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                    Label { text: "Company Name:" } TextField { id: companyNameField; Layout.fillWidth: true; placeholderText: "Registered company name"; selectByMouse: true }
                    Label { text: "GSTIN:" } TextField { id: companyGstinField; Layout.fillWidth: true; placeholderText: "GSTIN number"; selectByMouse: true }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; implicitHeight: companyAddressSection.implicitHeight + 16
            color: Theme.surfaceAlt; border.color: Theme.borderSubtle; radius: 4
            ColumnLayout {
                id: companyAddressSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                Label { text: "REGISTERED ADDRESS"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                GridLayout {
                    Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                    Label { text: "Address Line 1:" } TextField { id: companyAddr1Field; Layout.fillWidth: true; placeholderText: "Building, street, locality"; selectByMouse: true }
                    Label { text: "Address Line 2:" } TextField { id: companyAddr2Field; Layout.fillWidth: true; placeholderText: "Area or landmark"; selectByMouse: true }
                    Label { text: "City / State:" } TextField { id: companyCityField; Layout.fillWidth: true; Layout.columnSpan: 1; placeholderText: "City, state and PIN code"; selectByMouse: true }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; implicitHeight: companyContactSection.implicitHeight + 16
            color: Theme.surfaceAlt; border.color: Theme.borderSubtle; radius: 4
            ColumnLayout {
                id: companyContactSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                Label { text: "OFFICIAL CONTACT"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                GridLayout {
                    Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                    Label { text: "Phone:" } TextField { id: companyPhoneField; Layout.fillWidth: true; placeholderText: "Phone number"; inputMethodHints: Qt.ImhDialableCharactersOnly; selectByMouse: true }
                    Label { text: "Email:" } TextField { id: companyEmailField; Layout.fillWidth: true; placeholderText: "official@example.com"; inputMethodHints: Qt.ImhEmailCharactersOnly; selectByMouse: true }
                    Label { text: "Website:" } TextField { id: companyWebsiteField; Layout.fillWidth: true; Layout.columnSpan: 1; placeholderText: "www.example.com"; selectByMouse: true }
                }
            }
        }
    }
}
