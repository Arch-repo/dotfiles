import QtQuick
import QtQuick.Controls
ComboBox {
    id: control
    required property var theme
    implicitHeight: theme.tokens.size_login_control_height
    font.family: theme.font
    font.pixelSize: theme.tokens.font_body
    textRole: "name"
    leftPadding: theme.tokens.spacing_lg
    rightPadding: theme.tokens.spacing_xxl + theme.tokens.spacing_sm
    contentItem: Text {
        text: control.displayText
        font: control.font
        color: control.theme.foreground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Text {
        x: control.width - width - control.theme.tokens.spacing_md
        anchors.verticalCenter: parent.verticalCenter
        text: "⌄"
        color: control.theme.muted
        font.pixelSize: 18
    }
    background: Rectangle {
        color: control.theme.surface
        radius: control.theme.tokens.radius_control
        border.width: 1
        border.color: control.activeFocus ? control.theme.accent : control.theme.border
    }
    delegate: ItemDelegate {
        required property var model
        required property int index
        width: control.width
        text: model[control.textRole]
        font: control.font
        highlighted: control.highlightedIndex === index
        contentItem: Text { text: parent.text; font: control.font; color: control.theme.foreground; elide: Text.ElideRight }
        background: Rectangle { color: parent.highlighted ? control.theme.hover : "transparent"; radius: control.theme.tokens.radius_control }
    }
    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 16, 240)
        padding: 8
        background: Rectangle { color: control.theme.background; border.color: control.theme.border; radius: control.theme.tokens.radius_card }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator {}
        }
    }
}
