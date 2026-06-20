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
        window { width: 340px; border-radius: 24px; }
        listview { columns: 2; lines: 1; spacing: 12px; }
        element { padding: 14px; border-radius: 16px; children: [ element-text ]; }
        element-text { horizontal-align: 0.5; font: 'JetBrainsMono Nerd Font Bold 11'; }
    "

    choice="$(
        {
            printf 'Confirm\0'
            printf 'Cancel\0'
        } | /usr/bin/rofi -dmenu -i -p "$prompt" -mesg "$message" -theme "$THEME_MENU" -theme-str "$theme_str" -0
    )"

    [[ "$choice" == "Confirm" ]]
}

power_menu() {
    while true; do
        local choice
        local theme_str
        local title uptime_text session_text
        title="$(system_text "Power Menu")"
        uptime_text="$(uptime -p 2>/dev/null | sed 's/^up //')"
        session_text="${XDG_CURRENT_DESKTOP:-${DESKTOP_SESSION:-Hyprland}}"

        # Gorgeous circular/tile premium layout
        theme_str="
            window {
                width:            720px;
                border-radius:    28px;
                border:           1px;
                border-color:     @border-medium;
                background-color: @panel-bg;
                padding:          24px;
            }
            mainbox {
                spacing:          18px;
                children:         [ message, listview ];
            }
            message {
                border:           0px;
                padding:          0px 0px 8px 0px;
                background-color: transparent;
            }
            textbox {
                horizontal-align: 0.5;
                font:             'JetBrainsMono Nerd Font Medium 11';
                text-color:       @foreground;
            }
            listview {
                columns:          5;
                lines:            1;
                spacing:          14px;
                fixed-height:     true;
                scrollbar:        false;
                background-color: transparent;
            }
            element {
                orientation:      vertical;
                padding:          20px 8px;
                border-radius:    18px;
                border:           1px;
                border-color:     @border-soft;
                background-color: @item-bg;
                children:         [ element-text ];
            }
            element selected {
                background-color: @item-bg-active;
                border-color:     @accent-strong;
                text-color:       @foreground;
            }
            element-text {
                horizontal-align: 0.5;
                vertical-align:   0.5;
                text-align:       center;
                background-color: transparent;
                text-color:       inherit;
                markup:           true;
            }
        "

        choice="$(
            {
                printf "<span font='JetBrainsMono Nerd Font 32' foreground='${c_accent}'>󰌾</span>\n<span font='JetBrainsMono Nerd Font Bold 10.5'>Lock</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 32' foreground='${c_muted}'>󰈆</span>\n<span font='JetBrainsMono Nerd Font Bold 10.5'>Logout</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 32' foreground='${c_green}'>󰤄</span>\n<span font='JetBrainsMono Nerd Font Bold 10.5'>Suspend</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 32' foreground='${c_yellow}'>󰜉</span>\n<span font='JetBrainsMono Nerd Font Bold 10.5'>Reboot</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 32' foreground='${c_red}'></span>\n<span font='JetBrainsMono Nerd Font Bold 10.5'>Shutdown</span>\0"
            } | /usr/bin/rofi -dmenu -i -p "$title" -mesg "Session: ${session_text}  •  Uptime: ${uptime_text:-unknown}" -theme "$THEME_MENU" -theme-str "$theme_str" -markup-rows -0
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            *Lock*)
                hyprlock
                return 0
                ;;
            *Logout*)
                power_confirm "Log Out" "$(system_text "Log out?")" && hyprctl dispatch exit 0
                return 0
                ;;
            *Suspend*)
                systemctl suspend
                return 0
                ;;
            *Reboot*)
                power_confirm "Restart" "$(system_text "Restart?")" && systemctl reboot
                return 0
                ;;
            *Shutdown*)
                power_confirm "Power Off" "$(system_text "Power off?")" && systemctl poweroff
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}

power_menu
