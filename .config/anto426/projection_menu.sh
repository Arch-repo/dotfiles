#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"

notify() {
    notify-send "Schermi" "$*" 2>/dev/null || true
}

require_jq() {
    command -v jq >/dev/null 2>&1 || {
        notify "jq non trovato"
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
    notify "Solo schermo principale"
}

only_external() {
    local primary="$1" external="$2"
    [[ -n "$external" ]] || {
        notify "Nessuno schermo esterno rilevato"
        return 1
    }
    enable_monitor "$external" "0x0" 1
    disable_monitor "$primary"
    notify "Solo schermo esterno: $external"
}

extend_displays() {
    local primary="$1" external="$2" scale width
    [[ -n "$external" ]] || {
        notify "Nessuno schermo esterno rilevato"
        return 1
    }
    scale="$(scale_for "$primary")"
    width="$(logical_width "$primary")"
    enable_monitor "$primary" "0x0" "${scale:-1}"
    enable_monitor "$external" "${width:-1600}x0" 1
    notify "Schermi estesi"
}

duplicate_displays() {
    local primary="$1" external="$2" scale
    [[ -n "$external" ]] || {
        notify "Nessuno schermo esterno rilevato"
        return 1
    }
    scale="$(scale_for "$primary")"
    enable_monitor "$primary" "0x0" "${scale:-1}"
    hyprctl keyword monitor "$external,preferred,auto,1,mirror,$primary" >/dev/null 2>&1
    notify "Schermi duplicati"
}

require_jq

primary="$(primary_monitor)"
external="$(external_monitor "$primary")"

choice="$(
    printf '%s\n' \
        "󰍹 Solo schermo principale" \
        "󰍺 Duplica" \
        "󰹑 Estendi" \
        "󰶐 Solo schermo esterno" |
        rofi -dmenu -i -matching fuzzy \
            -p "Proietta" \
            -mesg "Principale: ${primary:-?}\nEsterno: ${external:-non rilevato}" \
            -theme "$THEME"
)"

case "$choice" in
    *"principale"*) only_primary "$primary" ;;
    *"Duplica"*) duplicate_displays "$primary" "$external" ;;
    *"Estendi"*) extend_displays "$primary" "$external" ;;
    *"esterno"*) only_external "$primary" "$external" ;;
esac
