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
        local theme_str
        message="$(main_dashboard_message)"
        
        # Dashboard grid: inherits the shared control surface and only changes layout.
        theme_str="
            window { width: 560px; }
            listview { columns: 3; lines: 5; spacing: 8px; }
            element { orientation: vertical; padding: 14px 9px; border-radius: 16px; }
            element-icon { size: 32px; margin: 0px 0px 6px 0px; horizontal-align: 0.5; }
            element-text { horizontal-align: 0.5; text-align: center; font: 'JetBrainsMono Nerd Font Bold 10.2'; }
        "

        choice="$(
            {
                printf 'Wi-Fi\0icon\x1fnetwork-wireless\n'
                printf 'Bluetooth\0icon\x1fpreferences-system-bluetooth\n'
                printf 'Audio\0icon\x1faudio-volume-high\n'
                printf 'Brightness\0icon\x1fdisplay-brightness\n'
                printf 'Battery\0icon\x1fbattery\n'
                printf 'Keyboard\0icon\x1finput-keyboard\n'
                printf 'Notifications\0icon\x1fpreferences-desktop-notification\n'
                printf 'Calendar\0icon\x1foffice-calendar\n'
                printf 'Display\0icon\x1fvideo-display\n'
                printf 'Wallpaper\0icon\x1fpreferences-desktop-wallpaper\n'
                printf 'Widgets\0icon\x1fpreferences-desktop-theme\n'
                printf 'Floating Manager\0icon\x1fwindow-restore\n'
                printf 'Power Menu\0icon\x1fsystem-shutdown\n'
            } | rofi_pick_msg "Dashboard" "$message" "$theme_str"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "Bluetooth") MENU_STATE="bluetooth"; return 0 ;;
            "Wi-Fi") MENU_STATE="wifi"; return 0 ;;
            "Audio") MENU_STATE="audio"; return 0 ;;
            "Brightness") ANTO426_MENU_PARENT=control run_script_or_notify "Brightness" "$HOME/.config/anto426/brightness_menu.sh"; return 0 ;;
            "Battery") MENU_STATE="battery"; return 0 ;;
            "Keyboard") MENU_STATE="keyboard"; return 0 ;;
            "Notifications") MENU_STATE="notifications"; return 0 ;;
            "Calendar") MENU_STATE="calendar"; return 0 ;;
            "Wallpaper") ANTO426_MENU_PARENT=control run_script_or_notify "Wallpaper" "$HOME/.config/anto426/wallpaper_select.sh"; return 0 ;;
            "Display") ANTO426_MENU_PARENT=control run_script_or_notify "Display" "$HOME/.config/anto426/projection_menu.sh"; return 0 ;;
            "Widgets") run_script_or_notify "Widgets" "$HOME/.config/anto426/widgets.sh" arrange; return 0 ;;
            "Floating Manager") ANTO426_MENU_PARENT=control run_script_or_notify "Floating Manager" "$HOME/.config/anto426/floating_manager.sh" menu; return 0 ;;
            "Power Menu") MENU_STATE="power"; return 0 ;;
            *) return 0 ;;
        esac
    done
}

main_menu
