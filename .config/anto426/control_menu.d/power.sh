#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

power_confirm() {
    local prompt="$1"
    local message="$2"
    local choice
    local theme_str

    theme_str="
        window { width: 340px; border-radius: 16px; }
        listview { columns: 2; lines: 1; spacing: 10px; }
        element { padding: 10px; border-radius: 8px; }
        element-text { horizontal-align: 0.5; font: 'JetBrainsMono Nerd Font Bold 10.5'; }
    "

    choice="$(
        {
            printf '%s\0icon\x1fbutton-ok\n' "$(menu_item "󰄬" "Confirm")"
            printf '%s\0icon\x1fbutton-cancel\n' "$(menu_item "󰅖" "Cancel")"
        } | rofi_pick_msg "$prompt" "$message" "$theme_str"
    )"

    [[ "$choice" == "$(system_text "Confirm")" ]]
}

power_menu() {
    while true; do
        local choice
        local title uptime_text session_text
        title="$(system_text "Power Menu")"
        uptime_text="$(uptime -p 2>/dev/null | sed 's/^up //')"
        session_text="${XDG_CURRENT_DESKTOP:-${DESKTOP_SESSION:-Hyprland}}"

        choice="$(
            {
                printf '%s\0icon\x1fsystem-lock-screen\n' "$(menu_item "󰌾" "Lock")"
                printf '%s\0icon\x1fsystem-log-out\n' "$(menu_item "󰍃" "Log Out")"
                printf '%s\0icon\x1fsystem-suspend\n' "$(menu_item "󰤄" "Suspend")"
                printf '%s\0icon\x1fsystem-reboot\n' "$(menu_item "󰑓" "Restart")"
                printf '%s\0icon\x1fsystem-shutdown\n' "$(menu_item "󰐥" "Power Off")"
                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "$title" "Session: ${session_text}\nUptime: ${uptime_text:-unknown}"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Lock")")
                hyprlock
                return 0
                ;;
            "$(system_text "Log Out")")
                power_confirm "Log Out" "$(system_text "Log out?")" && hyprctl dispatch exit 0
                return 0
                ;;
            "$(system_text "Suspend")")
                systemctl suspend
                return 0
                ;;
            "$(system_text "Restart")")
                power_confirm "Restart" "$(system_text "Restart?")" && systemctl reboot
                return 0
                ;;
            "$(system_text "Power Off")")
                power_confirm "Power Off" "$(system_text "Power off?")" && systemctl poweroff
                return 0
                ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
        esac
    done
}

power_menu
