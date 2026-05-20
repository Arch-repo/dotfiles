#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"

notify() {
    local value="${1:-}"
    if [[ "$value" =~ ^[0-9]+$ ]]; then
        notify-send -h "int:value:$value" "Luminosità" "${value}%" 2>/dev/null || true
    else
        notify-send "Luminosità" "$*" 2>/dev/null || true
    fi
}

current_percent() {
    brightnessctl -m 2>/dev/null | awk -F, '{gsub(/%/, "", $4); print int($4); exit}'
}

set_brightness() {
    local value="$1"

    command -v brightnessctl >/dev/null 2>&1 || {
        notify "brightnessctl non trovato"
        return 1
    }

    brightnessctl set "$value" >/dev/null 2>&1 || {
        notify "Operazione fallita"
        return 1
    }

    notify "$(current_percent)"
}

pick_action() {
    local current
    current="$(current_percent)"
    [[ -n "$current" ]] || current="?"

    printf '%s\n' \
        "󰃠 Aumenta +10%" \
        "󰃞 Diminuisci -10%" \
        "󱩎 25%" \
        "󱩏 50%" \
        "󱩐 75%" \
        "󰛨 100%" |
        rofi -dmenu -i -matching fuzzy \
            -p "Luminosità" \
            -mesg "Valore attuale: ${current}%" \
            -theme "$THEME"
}

case "${1:-menu}" in
    up) set_brightness "10%+" ;;
    down) set_brightness "10%-" ;;
    25 | 25%) set_brightness "25%" ;;
    50 | 50%) set_brightness "50%" ;;
    75 | 75%) set_brightness "75%" ;;
    100 | 100%) set_brightness "100%" ;;
    menu)
        choice="$(pick_action)"
        case "$choice" in
            *"Aumenta"*) set_brightness "10%+" ;;
            *"Diminuisci"*) set_brightness "10%-" ;;
            *"25%"*) set_brightness "25%" ;;
            *"50%"*) set_brightness "50%" ;;
            *"75%"*) set_brightness "75%" ;;
            *"100%"*) set_brightness "100%" ;;
        esac
        ;;
    *)
        printf 'Uso: %s [menu|up|down|25|50|75|100]\n' "$0" >&2
        exit 2
        ;;
esac
