import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "components"
Rectangle {
    id: root
    focus: true
    width: 1280
    height: 800
    color: shellTheme.background
    Tokens { id: designTokens }
    Theme { id: shellTheme; tokens: designTokens; settings: config }
    Image {
        id: wallpaper
        anchors.fill: parent
        source: config.stringValue("Background") || "Backgrounds/anto426-current.png"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        sourceSize.width: root.width
        sourceSize.height: root.height
        visible: true
    }
    MultiEffect {
        anchors.fill: wallpaper
        source: wallpaper
        blurEnabled: true
        blurMax: 48
        blur: 0.75
        autoPaddingEnabled: false
    }
    Rectangle { anchors.fill: parent; color: shellTheme.alpha(shellTheme.background, 0.48) }
    Flickable {
        anchors.fill: parent
        anchors.margins: designTokens.spacing_xxl
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        contentWidth: width
        contentHeight: Math.max(height, deck.implicitHeight)
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: deck
            width: Math.min(designTokens.size_auth_width, parent.width)
            x: (parent.width - width) / 2
            y: Math.max(0, (root.height - designTokens.spacing_xxl * 2 - implicitHeight) / 2)
            spacing: designTokens.spacing_xl
            Text {
                Layout.fillWidth: true
                text: Qt.formatTime(clock.now, "HH:mm")
                color: shellTheme.foreground
                font.family: shellTheme.font
                font.pixelSize: designTokens.font_widget_clock
                font.weight: Font.Light
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                Layout.fillWidth: true
                text: clock.now.toLocaleDateString(Qt.locale(shellTheme.locale), "dddd d MMMM")
                color: shellTheme.muted
                font.family: shellTheme.font
                font.pixelSize: designTokens.font_body
                horizontalAlignment: Text.AlignHCenter
            }
            GlassPanel {
                Layout.fillWidth: true
                implicitHeight: content.implicitHeight + designTokens.spacing_xxl * 2
                theme: shellTheme
                ColumnLayout {
                    id: content
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: designTokens.spacing_xxl }
                    spacing: designTokens.spacing_lg
                    LoginCard { id: loginForm; Layout.fillWidth: true; theme: shellTheme }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: designTokens.spacing_sm
                        LiquidButton { Layout.fillWidth: true; theme: shellTheme; text: "Sospendi"; visible: sddm.canSuspend; onClicked: sddm.suspend() }
                        LiquidButton { Layout.fillWidth: true; theme: shellTheme; text: "Riavvia"; visible: sddm.canReboot; onClicked: sddm.reboot() }
                        LiquidButton { Layout.fillWidth: true; theme: shellTheme; text: "Spegni"; visible: sddm.canPowerOff; onClicked: sddm.powerOff() }
                    }
                    LiquidPicker {
                        Layout.fillWidth: true
                        theme: shellTheme
                        model: keyboard.layouts
                        textRole: "longName"
                        visible: count > 1
                        currentIndex: keyboard.currentLayout
                        onActivated: keyboard.currentLayout = currentIndex
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: (sddm.hostName || "Il tuo desktop") + " · Arch Linux"
                color: shellTheme.muted
                font.family: shellTheme.font
                font.pixelSize: designTokens.font_caption
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.PlainText
            }
        }
    }
    Component.onCompleted: Qt.callLater(function() { loginForm.focusPassword() })
    Timer { id: clock; property date now: new Date(); interval: 1000; repeat: true; running: true; onTriggered: now = new Date() }
}
