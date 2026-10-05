import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: form
    objectName: "loginForm"
    required property var theme
    property bool pending: false
    property string message: ""
    property bool failed: false
    readonly property string username: users.count > 0 ? users.currentText : manualUser.text.trim()
    focus: true
    spacing: theme.tokens.spacing_md
    function focusPassword() { password.forceActiveFocus() }
    function submit() {
        if (pending || !username || sessions.currentIndex < 0 || (!password.text && !theme.settings.boolValue("AllowEmptyPassword"))) return
        message = ""
        failed = false
        pending = true
        sddm.login(username, password.text, sessions.currentIndex)
    }
    Text {
        Layout.fillWidth: true
        text: "Bentornato"
        color: form.theme.foreground
        font.family: form.theme.font
        font.pixelSize: form.theme.tokens.font_heading
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
    }
    Text {
        Layout.fillWidth: true
        text: "Il tuo desktop, pronto per te"
        color: form.theme.muted
        font.family: form.theme.font
        font.pixelSize: form.theme.tokens.font_body
        horizontalAlignment: Text.AlignHCenter
    }
    LiquidPicker {
        id: users
        objectName: "userPicker"
        Layout.fillWidth: true
        theme: form.theme
        model: userModel
        currentIndex: Math.max(0, userModel.lastIndex)
        visible: count > 0
        enabled: !form.pending
        onActivated: { password.text = ""; password.forceActiveFocus() }
    }
    TextField {
        id: manualUser
        Layout.fillWidth: true
        visible: users.count === 0
        enabled: !form.pending
        placeholderText: "Nome utente"
        color: form.theme.foreground
        placeholderTextColor: form.theme.muted
        font.family: form.theme.font
        font.pixelSize: form.theme.tokens.font_body
        padding: form.theme.tokens.spacing_md
        background: Rectangle { color: form.theme.surface; radius: form.theme.tokens.radius_control; border.color: manualUser.activeFocus ? form.theme.accent : form.theme.border }
        onAccepted: password.forceActiveFocus()
    }
    TextField {
        id: password
        objectName: "passwordInput"
        focus: true
        Layout.fillWidth: true
        implicitHeight: 48
        enabled: !form.pending
        echoMode: TextInput.Password
        passwordMaskDelay: 0
        placeholderText: "Password"
        color: form.theme.foreground
        placeholderTextColor: form.theme.muted
        selectionColor: form.theme.accent
        selectedTextColor: form.theme.selected
        font.family: form.theme.font
        font.pixelSize: form.theme.tokens.font_body
        padding: form.theme.tokens.spacing_md
        selectByMouse: true
        onAccepted: form.submit()
        Keys.onEscapePressed: { text = ""; form.message = "" }
        background: Rectangle {
            color: form.theme.surface
            radius: form.theme.tokens.radius_control
            border.width: 1
            border.color: password.activeFocus ? form.theme.accent : form.theme.border
        }
        Component.onCompleted: forceActiveFocus()
    }
    Text {
        Layout.fillWidth: true
        visible: keyboard.capsLock || form.message.length > 0
        text: form.message.length ? form.message : "Bloc Maiusc attivo"
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        color: form.failed ? form.theme.error : form.theme.muted
        font.family: form.theme.font
        font.pixelSize: form.theme.tokens.font_caption
    }
    LiquidButton {
        objectName: "loginButton"
        Layout.fillWidth: true
        theme: form.theme
        primary: true
        text: form.pending ? "Accesso in corso…" : (!password.text && form.theme.settings.boolValue("AllowEmptyPassword") ? "Accedi · impronta o password" : "Accedi")
        enabled: !form.pending && form.username.length > 0 && sessions.currentIndex >= 0
        onClicked: form.submit()
    }
    LiquidPicker {
        id: sessions
        objectName: "sessionPicker"
        Layout.fillWidth: true
        theme: form.theme
        model: sessionModel
        currentIndex: count > 0 ? Math.max(0, sessionModel.lastIndex) : -1
        enabled: !form.pending
    }
    Text {
        Layout.fillWidth: true
        visible: sessions.count === 0
        text: "Nessuna sessione disponibile"
        color: form.theme.error
        font.pixelSize: form.theme.tokens.font_caption
    }
    Connections {
        target: sddm
        function onLoginFailed() {
            form.pending = false
            form.failed = true
            password.text = ""
            form.message = "Accesso non riuscito. Riprova."
            password.forceActiveFocus()
        }
        function onInformationMessage(message) { form.failed = false; form.message = message }
        function onLoginSucceeded() { password.text = "" }
    }
}
