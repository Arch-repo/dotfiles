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
    local confirm="Confirm"
    local cancel="Cancel"
    local theme_str

    theme_str="
        window { width: 340px; border-radius: 24px; }
        listview { columns: 2; lines: 1; spacing: 10px; }
        element { padding: 12px; border-radius: 14px; }
        element-icon { size: 24px; }
        element-text { horizontal-align: 0.5; font: 'JetBrainsMono Nerd Font Bold 11'; }
    "

    choice="$(
        {
            printf 'Confirm\0icon\x1fbutton-ok\n'
            printf 'Cancel\0icon\x1fbutton-cancel\n'
        } | rofi_pick_msg "$prompt" "$message" "$theme_str"
    )"

    [[ "$choice" == "$confirm" ]]
}

power_menu() {
    while true; do
        local choice
        local theme_str
        local title uptime_text session_text
        title="$(system_text "Power Off")"
        uptime_text="$(uptime -p 2>/dev/null | sed 's/^up //')"
        session_text="${XDG_CURRENT_DESKTOP:-${DESKTOP_SESSION:-Hyprland}}"

        # Horizontal action layout
        theme_str="
            window { width: 640px; border-radius: 24px; }
            listview { columns: 6; lines: 1; spacing: 10px; }
            element { orientation: vertical; padding: 18px 8px; border-radius: 16px; }
            element-icon { size: 38px; horizontal-align: 0.5; }
            element-text { horizontal-align: 0.5; text-align: center; font: 'JetBrainsMono Nerd Font Bold 10.5'; }
        "

        choice="$(
            {
                printf 'Lock\0icon\x1fsystem-lock-screen\n'
                printf 'Log Out\0icon\x1fsystem-log-out\n'
                printf 'Suspend\0icon\x1fsystem-suspend\n'
                printf 'Restart\0icon\x1fsystem-reboot\n'
                printf 'Power Off\0icon\x1fsystem-shutdown\n'
                printf 'Back\0icon\x1fgo-previous\n'
            } | rofi_pick_msg "$title" "Session: ${session_text}\nUptime: ${uptime_text:-unknown}" "$theme_str"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "Lock")
                hyprlock
                return 0
                ;;
            "Log Out")
                power_confirm "Log Out" "$(system_text "Log out?")" && hyprctl dispatch exit 0
                return 0
                ;;
            "Suspend")
                systemctl suspend
                return 0
                ;;
            "Restart")
                power_confirm "Restart" "$(system_text "Restart?")" && systemctl reboot
                return 0
                ;;
            "Power Off")
                power_confirm "Power Off" "$(system_text "Power off?")" && systemctl poweroff
                return 0
                ;;
            "Back")
                back_or_main
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}

power_menu
