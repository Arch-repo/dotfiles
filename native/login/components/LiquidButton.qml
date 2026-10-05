import QtQuick
import QtQuick.Controls
Button {
    id: control
    required property var theme
    property bool primary: false
    implicitHeight: theme.tokens.size_login_control_height
    font.family: theme.font
    font.pixelSize: theme.tokens.font_body
    padding: theme.tokens.spacing_md
    opacity: enabled ? 1 : 0.55
    contentItem: Text {
        text: control.text
        font: control.font
        color: control.primary ? control.theme.selected : control.theme.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: control.theme.tokens.radius_control
        color: control.primary ? control.theme.accent : (control.down || control.hovered ? control.theme.hover : control.theme.surface)
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? control.theme.accent : control.theme.border
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
