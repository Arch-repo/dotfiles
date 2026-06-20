#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"

notify() {
    notify-send "Schermi" "$*" 2>/dev/null || true
}

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    exit 0
}

require_jq() {
    command -v jq >/dev/null 2>&1 || {
        notify "jq non trovato"
        exit 1
    }
}

require_hyprctl() {
    command -v hyprctl >/dev/null 2>&1 || {
        notify "hyprctl non trovato"
        exit 1
    }
}

monitors_json() {
    hyprctl monitors all -j 2>/dev/null
}

monitor_names() {
    monitors_json | jq -r '.[].name'
}

primary_monitor() {
    monitors_json |
        jq -r '([.[] | select(.name | test("^(eDP|LVDS|DSI)"))][0] // [.[] | select(.focused == true)][0] // .[0]).name'
}

external_monitor() {
    local primary="$1"
    monitors_json |
        jq -r --arg primary "$primary" '[.[] | select(.name != $primary)][0].name // empty'
}

logical_width() {
    local monitor="$1"
    monitors_json |
        jq -r --arg monitor "$monitor" '.[] | select(.name == $monitor) | ((.width / .scale) | floor)' |
        sed -n '1p'
}

scale_for() {
    local monitor="$1"
    monitors_json |
        jq -r --arg monitor "$monitor" '.[] | select(.name == $monitor) | .scale' |
        sed -n '1p'
}

enable_monitor() {
    local monitor="$1"
    local pos="$2"
    local scale="${3:-1}"
    hyprctl keyword monitor "$monitor,preferred,$pos,$scale" >/dev/null 2>&1
}

disable_monitor() {
    local monitor="$1"
    hyprctl keyword monitor "$monitor,disable" >/dev/null 2>&1
}

only_primary() {
    local primary="$1" monitor scale
    scale="$(scale_for "$primary")"
    enable_monitor "$primary" "0x0" "${scale:-1}"
    while IFS= read -r monitor; do
        [[ "$monitor" == "$primary" ]] && continue
        disable_monitor "$monitor"
    done < <(monitor_names)
    notify "Main screen only"
}

only_external() {
    local primary="$1" external="$2"
    [[ -n "$external" ]] || {
        notify "No external screen detected"
        return 1
    }
    enable_monitor "$external" "0x0" 1
    disable_monitor "$primary"
    notify "External screen only: $external"
}

extend_displays() {
    local primary="$1" external="$2" scale width
    [[ -n "$external" ]] || {
        notify "No external screen detected"
        return 1
    }
    scale="$(scale_for "$primary")"
    width="$(logical_width "$primary")"
    enable_monitor "$primary" "0x0" "${scale:-1}"
    enable_monitor "$external" "${width:-1600}x0" 1
    notify "Displays extended"
}

duplicate_displays() {
    local primary="$1" external="$2" scale
    [[ -n "$external" ]] || {
        notify "No external screen detected"
        return 1
    }
    scale="$(scale_for "$primary")"
    enable_monitor "$primary" "0x0" "${scale:-1}"
    hyprctl keyword monitor "$external,preferred,auto,1,mirror,$primary" >/dev/null 2>&1
    notify "Displays duplicated"
}

require_hyprctl
require_jq

primary="$(primary_monitor)"
external="$(external_monitor "$primary")"
[[ -n "$primary" ]] || {
    notify "No screen detected"
    exit 1
}

while true; do
    choice="$(
        {
            printf 'Main screen only\0icon\x1fvideo-display\n'
            printf 'Duplicate\0icon\x1fvideo-display\n'
            printf 'Extend\0icon\x1fvideo-display\n'
            printf 'External screen only\0icon\x1fvideo-display\n'
            printf 'Back\0icon\x1fgo-previous\n'
        } |
            rofi -dmenu -i -matching fuzzy \
                -p "Project" \
                -mesg "Primary: ${primary:-?}\nExternal: ${external:-not detected}" \
                -theme "$THEME"
    )"

    [[ -z "$choice" ]] && exit 0

    case "$choice" in
        "Main screen only") only_primary "$primary"; exit 0 ;;
        "Duplicate") duplicate_displays "$primary" "$external"; exit 0 ;;
        "Extend") extend_displays "$primary" "$external"; exit 0 ;;
        "External screen only") only_external "$primary" "$external"; exit 0 ;;
        "Back") go_back ;;
    esac
done
