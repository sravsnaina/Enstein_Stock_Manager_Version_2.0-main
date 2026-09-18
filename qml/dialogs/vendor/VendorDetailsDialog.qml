import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  VendorDetailsDialog - edit one vendor
// ===========================================================================
//  A vendor is identified by name everywhere in the application, so the name it
//  had when the dialog opened is kept in originalName: that is what the backend
//  matches on, which is what makes renaming a vendor possible at all.
// ===========================================================================
Dialog {
    // Raised for Main.qml to act on: this dialog cannot see its
    // siblings, so anything involving another screen goes out as a signal.
    signal vendorChanged()
    signal dropdownsStale()
    property var departmentOptions: Departments.options([])

    title: "Vendor Details"
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.Ok | Dialog.Cancel
    // Wide enough for three field pairs per row, and never taller than
    // the window: the fields scroll inside rather than pushing the OK
    // and Cancel buttons off the bottom edge.
    width: Math.min(App.windowWidth - 120, 1100)
    height: Math.min(App.windowHeight - 120, 540)

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        Label { text: "Edit Vendor Details"; font.bold: true; font.pixelSize: 14 }
        Rectangle { Layout.fillWidth: true; height: 1; color: "#ccc" }

        ScrollView {
            id: editVendorScroll
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: editVendorScroll.availableWidth
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: editProfileSection.implicitHeight + 16
                    color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                    ColumnLayout {
                        id: editProfileSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                        Label { text: "VENDOR PROFILE & CONTACT"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                        GridLayout {
                            Layout.fillWidth: true; columns: 4; rowSpacing: 8; columnSpacing: 10
                            Label { text: "Vendor Name:" } TextField { id: editVendorNameField; Layout.fillWidth: true; placeholderText: "Registered vendor name"; selectByMouse: true }
                            Label { text: "Department:" } ComboBox { id: editVendorDepartmentField; Layout.fillWidth: true; model: departmentOptions; currentIndex: -1 }
                            Label { text: "Contact Person:" } TextField { id: editVendorContactPersonField; Layout.fillWidth: true; placeholderText: "Primary contact name"; selectByMouse: true }
                            Label { text: "Phone No.:" } TextField { id: editVendorPhoneNumberField; Layout.fillWidth: true; placeholderText: "Phone number"; inputMethodHints: Qt.ImhDialableCharactersOnly; selectByMouse: true }
                            Label { text: "Email:" } TextField { id: editVendorEmailField; Layout.fillWidth: true; Layout.columnSpan: 3; placeholderText: "official@example.com"; inputMethodHints: Qt.ImhEmailCharactersOnly; selectByMouse: true }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: editAddressSection.implicitHeight + 16
                    color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                    ColumnLayout {
                        id: editAddressSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                        Label { text: "REGISTERED ADDRESS"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                        TextArea { id: editVendorAddressField; Layout.fillWidth: true; Layout.preferredHeight: 72; placeholderText: "Building, street, locality, city, state and PIN code"; wrapMode: TextEdit.Wrap; selectByMouse: true }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true; spacing: 8
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: editStatutorySection.implicitHeight + 16
                        color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                        ColumnLayout {
                            id: editStatutorySection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                            Label { text: "TAX & REGISTRATION"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                            GridLayout {
                                Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                                Label { text: "GSTIN:" } TextField { id: editVendorGstinField; Layout.fillWidth: true; placeholderText: "GSTIN number"; selectByMouse: true }
                                Label { text: "PAN:" } TextField { id: editVendorPANField; Layout.fillWidth: true; placeholderText: "PAN number"; selectByMouse: true }
                                Label { text: "CIN:" } TextField { id: editVendorCinField; Layout.fillWidth: true; placeholderText: "CIN number"; selectByMouse: true }
                                Label { text: "PAN Name:" } TextField { id: editVendorPanNameField; Layout.fillWidth: true; placeholderText: "Name as per PAN card"; selectByMouse: true }
                                Label { text: "Supply Category:" } TextField { id: editVendorItemCategoryField; Layout.fillWidth: true; placeholderText: "Products or services supplied"; selectByMouse: true }
                            }
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true; implicitHeight: editBankSection.implicitHeight + 16
                        color: Theme.surface; border.color: Theme.borderSubtle; radius: 4
                        ColumnLayout {
                            id: editBankSection; anchors.fill: parent; anchors.margins: 8; spacing: 6
                            Label { text: "BANK DETAILS"; font.bold: true; font.pixelSize: 11; color: Theme.textSecondary }
                            GridLayout {
                                Layout.fillWidth: true; columns: 2; rowSpacing: 8; columnSpacing: 10
                                Label { text: "Bank & Branch:" } TextField { id: editVendorBankBranchField; Layout.fillWidth: true; placeholderText: "Bank and branch name"; selectByMouse: true }
                                Label { text: "Account Number:" } TextField { id: editVendorAccountField; Layout.fillWidth: true; placeholderText: "Bank account number"; inputMethodHints: Qt.ImhDigitsOnly; selectByMouse: true }
                                Label { text: "IFSC Code:" } TextField { id: editVendorIfscField; Layout.fillWidth: true; placeholderText: "IFSC code"; selectByMouse: true }
                            }
                        }
                    }
                }
            }
        }
    }

    onAccepted: {
        var vendorDetails = {
            originalName: originalName,
            vendorName: editVendorNameField.text,
            vendorAddress: editVendorAddressField.text,
            bankBranch: editVendorBankBranchField.text,
            ifsc: editVendorIfscField.text,
            accountNumber: editVendorAccountField.text,
            cin: editVendorCinField.text,
            gstin: editVendorGstinField.text,
            panNumber: editVendorPANField.text,
            panName: editVendorPanNameField.text,
            contactPerson: editVendorContactPersonField.text,
            email: editVendorEmailField.text,
            phone: editVendorPhoneNumberField.text,
            department: editVendorDepartmentField.currentText,
            itemCategory: editVendorItemCategoryField.text
        }

        if (Backend.updateVendorDetails(vendorDetails)) {
            vendorChanged()
            dropdownsStale()
        }
    }

    function showVendor(vendor) {
        originalName = vendor.name || ""
        editVendorNameField.text = vendor.name || ""
        editVendorAddressField.text = vendor.address || ""
        editVendorBankBranchField.text = vendor.bankBranch || ""
        editVendorIfscField.text = vendor.ifsc || ""
        editVendorAccountField.text = vendor.accountNumber || ""
        editVendorCinField.text = vendor.cin || ""
        editVendorGstinField.text = vendor.gstin || ""
        editVendorPANField.text = vendor.pan || ""
        editVendorPanNameField.text = vendor.panName || ""
        editVendorContactPersonField.text = vendor.contactPerson || ""
        editVendorEmailField.text = vendor.email || ""
        editVendorPhoneNumberField.text = vendor.phone || ""
        departmentOptions = Departments.options(Backend.getItemMasterList())
        editVendorDepartmentField.currentIndex = Departments.indexOf(departmentOptions, vendor.department || "")
        editVendorItemCategoryField.text = vendor.itemCategory || ""
        // This function belongs to the dialog itself.  Referencing the ID of
        // the instance created in Main.qml is outside this component's scope,
        // so the edit dialog can fail to open before the submitted values ever
        // reach updateVendorDetails().
        open()
    }
    property string originalName: ""
}
