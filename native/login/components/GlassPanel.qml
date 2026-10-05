import QtQuick
Rectangle {
    required property var theme
    color: theme.panel
    radius: theme.tokens.radius_panel
    border.width: 1
    border.color: theme.border
}
