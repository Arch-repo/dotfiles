#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

main_wifi_status() {
    local enabled connected
    enabled="$(nmcli radio wifi 2>/dev/null || true)"
    connected="$(
        nmcli -t -f TYPE,NAME connection show --active 2>/dev/null |
            awk -F: '$1 == "802-11-wireless" {print $2; exit}'
    )"

    if [[ "$enabled" != "enabled" ]]; then
        printf 'off'
    else
        printf '%s' "${connected:-on, not connected}"
    fi
}

main_bluetooth_status() {
    local powered connected
    powered="$(bluetoothctl show 2>/dev/null | awk -F': ' '/Powered:/ {print $2; exit}')"
    [[ "$powered" == "yes" ]] || {
        printf 'off'
        return 0
    }

    connected="$(
        bluetoothctl devices Connected 2>/dev/null |
            awk '{name=$0; sub(/^Device [^ ]+ /, "", name); print name; exit}'
    )"
    printf '%s' "${connected:-on, no device}"
}

main_battery_status() {
    local supply capacity status
    for supply in /sys/class/power_supply/*; do
        [[ -r "$supply/type" ]] || continue
        [[ "$(cat "$supply/type" 2>/dev/null)" == "Battery" ]] || continue
        capacity="$(cat "$supply/capacity" 2>/dev/null)"
        status="$(cat "$supply/status" 2>/dev/null)"
        printf '%s%% %s' "${capacity:-?}" "${status:-unknown}"
        return 0
    done

    printf 'not detected'
}

main_audio_status() {
    local volume
    volume="$(
        wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null |
            awk '/^Volume:/ {printf "%d%%", ($2 * 100) + 0.5; if ($0 ~ /\[MUTED\]/) printf " muted"; exit}'
    )"
    printf '%s' "${volume:-unknown}"
}

main_brightness_status() {
    local brightness
    brightness="$(brightnessctl -m 2>/dev/null | awk -F, '{gsub(/%/, "", $4); print int($4) "%"; exit}')"
    printf '%s' "${brightness:-unknown}"
}

main_dashboard_message() {
    printf '<b>Wi-Fi</b>: <span foreground="%s">%s</span>\n' "$c_cyan" "$(main_wifi_status)"
    printf '<b>Bluetooth</b>: <span foreground="%s">%s</span>\n' "$c_accent" "$(main_bluetooth_status)"
    printf '<b>Audio</b>: <span foreground="%s">%s</span>\n' "$c_yellow" "$(main_audio_status)"
    printf '<b>Brightness</b>: <span foreground="%s">%s</span>\n' "$c_yellow" "$(main_brightness_status)"
    printf '<b>Battery</b>: <span foreground="%s">%s</span>' "$c_green" "$(main_battery_status)"
}

main_menu() {
    while true; do
        local choice message
        message="$(main_dashboard_message)"

        choice="$(
            {
                printf '%s\0icon\x1fnetwork-wireless\n' "$(system_text "Wi-Fi")"
                printf '%s\0icon\x1fpreferences-system-bluetooth\n' "$(system_text "Bluetooth")"
                printf '%s\0icon\x1faudio-volume-high\n' "$(system_text "Audio")"
                printf '%s\0icon\x1fdisplay-brightness\n' "$(system_text "Brightness")"
                printf '%s\0icon\x1fbattery\n' "$(system_text "Battery")"
                printf '%s\0icon\x1finput-keyboard\n' "$(system_text "Keyboard")"
                printf '%s\0icon\x1fpreferences-desktop-notification\n' "$(system_text "Notifications")"
                printf '%s\0icon\x1foffice-calendar\n' "$(system_text "Calendar")"
                printf '%s\0icon\x1fcomputer\n' "$(system_text "Hardware")"
                printf '%s\0icon\x1fvideo-display\n' "$(system_text "Display")"
                printf '%s\0icon\x1fpreferences-desktop-wallpaper\n' "$(system_text "Wallpaper")"
                printf '%s\0icon\x1fpreferences-desktop-theme\n' "$(system_text "Widgets")"
                printf '%s\0icon\x1fwindow-restore\n' "$(system_text "Floating Manager")"
                printf '%s\0icon\x1fsystem-shutdown\n' "$(system_text "Power Menu")"
            } | rofi_pick_msg "Dashboard" "$message"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Bluetooth")") MENU_STATE="bluetooth"; return 0 ;;
            "$(system_text "Wi-Fi")") MENU_STATE="wifi"; return 0 ;;
            "$(system_text "Audio")") MENU_STATE="audio"; return 0 ;;
            "$(system_text "Brightness")") ANTO426_MENU_PARENT=control run_script_or_notify "Brightness" "$HOME/.config/anto426/brightness_menu.sh"; return 0 ;;
            "$(system_text "Battery")") MENU_STATE="battery"; return 0 ;;
            "$(system_text "Keyboard")") MENU_STATE="keyboard"; return 0 ;;
            "$(system_text "Notifications")") MENU_STATE="notifications"; return 0 ;;
            "$(system_text "Calendar")") MENU_STATE="calendar"; return 0 ;;
            "$(system_text "Hardware")") ANTO426_MENU_PARENT=control run_script_or_notify "Hardware" "$HOME/.config/anto426/hardware_stats.sh" menu; return 0 ;;
            "$(system_text "Wallpaper")") ANTO426_MENU_PARENT=control run_script_or_notify "Wallpaper" "$HOME/.config/anto426/wallpaper_select.sh"; return 0 ;;
            "$(system_text "Display")") ANTO426_MENU_PARENT=control run_script_or_notify "Display" "$HOME/.config/anto426/projection_menu.sh"; return 0 ;;
            "$(system_text "Widgets")") run_script_or_notify "Widgets" "$HOME/.config/anto426/widgets.sh" arrange; return 0 ;;
            "$(system_text "Floating Manager")") ANTO426_MENU_PARENT=control run_script_or_notify "Floating Manager" "$HOME/.config/anto426/floating_manager.sh" menu; return 0 ;;
            "$(system_text "Power Menu")") MENU_STATE="power"; return 0 ;;
            *) return 0 ;;
        esac
    done
}

main_menu

