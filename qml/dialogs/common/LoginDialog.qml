import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ExcelHandler 1.0
import Enstein

// ===========================================================================
//  LoginDialog - who is using the application
// ===========================================================================
//  Signing in sets the name stamped onto documents and the role that decides
//  whether anything may be edited. isAuthenticated is read by the title bar.
// ===========================================================================
Dialog {
    title: "Authorization Required"
    modal: true
    standardButtons: Dialog.Ok | Dialog.Cancel
    anchors.centerIn: parent
    property bool isAuthenticated: false

    ColumnLayout {
        spacing: 15
        Label { text: "Enter credentials to enable editing"; font.bold: true; font.pixelSize: 14 }
        Label { text: "Username:" }
        TextField { id: usernameField; placeholderText: "admin"; Layout.fillWidth: true; selectByMouse: true }
        Label { text: "Password:" }
        TextField {
            id: passwordField; placeholderText: "password"; echoMode: TextInput.Password
            Layout.fillWidth: true; selectByMouse: true
            Keys.onReturnPressed: loginDialog.accept()
        }
        Label { id: loginError; text: ""; color: Theme.danger; visible: text !== "" }
    }

    onAccepted: {
        // Credentials are verified against the shared users table in the database.
        var role = Backend.login(usernameField.text, passwordField.text)
        if (role !== "") {
            isAuthenticated = true
            loginError.text = ""
            usernameField.text = ""; passwordField.text = ""
            statusLabel.text = "Login successful (" + role + ")"; statusTimer.restart()
        } else {
            isAuthenticated = false
            loginError.text = "Invalid credentials"; passwordField.text = ""
            loginDialog.open()
        }
    }
    
    onOpened: loginError.text = ""
}
