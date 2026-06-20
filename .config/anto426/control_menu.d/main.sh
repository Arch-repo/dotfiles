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
    local wifi bt audio bright batt
    wifi="$(main_wifi_status)"
    bt="$(main_bluetooth_status)"
    audio="$(main_audio_status)"
    bright="$(main_brightness_status)"
    batt="$(main_battery_status)"

    printf '󰤨  <b>Wi-Fi</b>: <span foreground="%s">%s</span>    󰂯  <b>Bluetooth</b>: <span foreground="%s">%s</span>\n' "$c_cyan" "$wifi" "$c_accent" "$bt"
    printf '  <b>Audio</b>: <span foreground="%s">%s</span>    󰃠  <b>Brightness</b>: <span foreground="%s">%s</span>\n' "$c_yellow" "$audio" "$c_yellow" "$bright"
    printf '  <b>Battery</b>: <span foreground="%s">%s</span>' "$c_green" "$batt"
}

main_menu() {
    while true; do
        local choice message
        local theme_str
        message="$(main_dashboard_message)"
        
        # Gorgeous circular/tile premium grid layout
        theme_str="
            window {
                width:            620px;
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
                border:           1px;
                border-color:     @border-soft;
                border-radius:    16px;
                padding:          12px 16px;
                background-color: @item-bg;
            }
            textbox {
                horizontal-align: 0.0;
                font:             'JetBrainsMono Nerd Font Medium 10';
                text-color:       @foreground;
                line-spacing:     4px;
            }
            listview {
                columns:          3;
                lines:            5;
                spacing:          12px;
                fixed-height:     true;
                scrollbar:        false;
                background-color: transparent;
            }
            element {
                orientation:      vertical;
                padding:          16px 8px;
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
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_cyan}'>󰤨</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Wi-Fi</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰂯</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Bluetooth</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_yellow}'></span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Audio</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_yellow}'>󰃠</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Brightness</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_green}'></span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Battery</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰌌</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Keyboard</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰂚</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Notifications</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰃭</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Calendar</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰍹</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Display</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰸉</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Wallpaper</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰘦</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Widgets</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰖲</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Floating</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_accent}'>󰌾</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Lock Screen</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_muted}'>󰙔</span>\n<span font='JetBrainsMono Nerd Font Bold 10'>System Info</span>\0"
                printf "<span font='JetBrainsMono Nerd Font 28' foreground='${c_red}'></span>\n<span font='JetBrainsMono Nerd Font Bold 10'>Power Menu</span>\0"
            } | /usr/bin/rofi -dmenu -i -p "Dashboard" -mesg "$message" -theme "$THEME_MENU" -theme-str "$theme_str" -markup-rows -0
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            *"Wi-Fi"*) MENU_STATE="wifi"; return 0 ;;
            *"Bluetooth"*) MENU_STATE="bluetooth"; return 0 ;;
            *"Audio"*) MENU_STATE="audio"; return 0 ;;
            *"Brightness"*) ANTO426_MENU_PARENT=control run_script_or_notify "Brightness" "$HOME/.config/anto426/brightness_menu.sh"; return 0 ;;
            *"Battery"*) MENU_STATE="battery"; return 0 ;;
            *"Keyboard"*) MENU_STATE="keyboard"; return 0 ;;
            *"Notifications"*) MENU_STATE="notifications"; return 0 ;;
            *"Calendar"*) MENU_STATE="calendar"; return 0 ;;
            *"Display"*) ANTO426_MENU_PARENT=control run_script_or_notify "Display" "$HOME/.config/anto426/projection_menu.sh"; return 0 ;;
            *"Wallpaper"*) ANTO426_MENU_PARENT=control run_script_or_notify "Wallpaper" "$HOME/.config/anto426/wallpaper_select.sh"; return 0 ;;
            *"Widgets"*) run_script_or_notify "Widgets" "$HOME/.config/anto426/widgets.sh" arrange; return 0 ;;
            *"Floating"*) ANTO426_MENU_PARENT=control run_script_or_notify "Floating Manager" "$HOME/.config/anto426/floating_manager.sh" menu; return 0 ;;
            *"Lock Screen"*) hyprlock; return 0 ;;
            *"System Info"*)
                local sys_info
                sys_info="Host: $(hostname)\nOS: Arch Linux\nKernel: $(uname -r)\nShell: $SHELL\nCPU: $(lscpu 2>/dev/null | awk -F': +' '/Model name/ {print $2; exit}')\nMemory: $(free -h | awk '/Mem:/ {print $3 "/" $2}')"
                /usr/bin/rofi -e "$sys_info" -theme "$THEME_MENU"
                ;;
            *"Power"*) MENU_STATE="power"; return 0 ;;
            *) return 0 ;;
        esac
    done
}

main_menu
