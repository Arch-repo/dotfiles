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

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    exit 0
}

brightness_bar() {
    local value="$1"
    local filled=$((value / 10))
    local bar=""
    local i

    for ((i = 0; i < 10; i++)); do
        if ((i < filled)); then
            bar+="━"
        else
            bar+="─"
        fi
    done

    printf '%s' "$bar"
}

set_brightness() {
    local value="$1"
    local before after

    command -v brightnessctl >/dev/null 2>&1 || {
        notify "brightnessctl non trovato"
        return 1
    }

    before="$(current_percent)"
    [[ -n "$before" ]] || before=0

    brightnessctl set "$value" >/dev/null 2>&1 || {
        notify "Operazione fallita"
        return 1
    }

    after="$(current_percent)"
    [[ -n "$after" ]] || after="$before"
    [[ "$after" == "$before" ]] && return 0

    notify "$after"
}

adjust_brightness() {
    local direction="$1"
    local current

    command -v brightnessctl >/dev/null 2>&1 || {
        notify "brightnessctl non trovato"
        return 1
    }

    current="$(current_percent)"
    [[ -n "$current" ]] || current=0

    case "$direction" in
        up)
            ((current >= 100)) && return 0
            set_brightness "10%+"
            ;;
        down)
            ((current <= 0)) && return 0
            set_brightness "10%-"
            ;;
        *)
            return 1
            ;;
    esac
}

pick_action() {
    local current selected pct marker
    current="$(current_percent)"
    [[ -n "$current" ]] || current=0
    selected=$((7 + ((current + 2) / 5)))

    {
        printf '%s\n' \
            "󰃠 Aumenta +10%" \
            "󰃞 Diminuisci -10%" \
            "󱩎 25%" \
            "󱩏 50%" \
            "󱩐 75%" \
            "󰛨 100%" \
            "── Valori ──"
        for pct in $(seq 0 5 100); do
            marker=" "
            ((pct == ((current + 2) / 5) * 5)) && marker="*"
            printf '%s %3d%%  %s\n' "$marker" "$pct" "$(brightness_bar "$pct")"
        done
        printf '%s\n' "󰌍 Indietro"
    } |
        rofi -dmenu -i -matching fuzzy \
            -p "Luminosità" \
            -mesg "Valore attuale: ${current}%" \
            -selected-row "$selected" \
            -theme "$THEME"
}

case "${1:-menu}" in
    up) adjust_brightness up ;;
    down) adjust_brightness down ;;
    25 | 25%) set_brightness "25%" ;;
    50 | 50%) set_brightness "50%" ;;
    75 | 75%) set_brightness "75%" ;;
    100 | 100%) set_brightness "100%" ;;
    menu)
        choice="$(pick_action)"
        case "$choice" in
            *"Aumenta"*) adjust_brightness up ;;
            *"Diminuisci"*) adjust_brightness down ;;
            *"Valori"*) exit 0 ;;
            *"Indietro"*) go_back ;;
            *"% "* | *"%")
                if [[ "$choice" =~ ([0-9]{1,3})% ]]; then
                    set_brightness "$((10#${BASH_REMATCH[1]}))%"
                fi
                ;;
        esac
        ;;
    *)
        printf 'Uso: %s [menu|up|down|25|50|75|100]\n' "$0" >&2
        exit 2
        ;;
esac
