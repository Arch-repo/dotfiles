import QtQuick
QtObject {
    required property QtObject tokens
    required property var settings
    function setting(key, fallback) {
        const value = settings.stringValue(key)
        return value.length ? value : fallback
    }
    function alpha(value, opacity) { return Qt.rgba(value.r, value.g, value.b, opacity) }
    function mix(a, b, amount) {
        return Qt.rgba(a.r*(1-amount)+b.r*amount, a.g*(1-amount)+b.g*amount, a.b*(1-amount)+b.b*amount, 1)
    }
    readonly property color background: setting("BackgroundColor", "#141820")
    readonly property color foreground: setting("MainColor", "#edf1f8")
    readonly property color accent: setting("AccentColor", "#b4c8ff")
    readonly property color muted: setting("MutedColor", "#a1adbf")
    readonly property color error: setting("ErrorColor", "#f3a1aa")
    readonly property color selected: setting("OverrideLoginButtonTextColor", "#111722")
    readonly property color panel: alpha(mix(Qt.darker(background, 1/0.75), accent, 0.14), tokens.material_opacity)
    readonly property color surface: alpha(foreground, 0.065)
    readonly property color hover: alpha(foreground, 0.105)
    readonly property color border: alpha(mix(foreground, accent, 0.20), 0.21)
    readonly property string font: "Noto Sans"
    readonly property string locale: setting("Locale", "it_IT")
}
