#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSD_SCRIPT="$SCRIPT_DIR/osd_show.sh"
# shellcheck source=rofi_slider.sh
source "$SCRIPT_DIR/rofi_slider.sh"

show_osd() {
    local value="${1:-0}"
    [[ -x "$OSD_SCRIPT" ]] && "$OSD_SCRIPT" brightness "$value" 0
}

notify() {
    local value="${1:-}"
    if [[ "$value" =~ ^[0-9]+$ ]]; then
        show_osd "$value"
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

set_brightness() {
    local value="$1"
    local before after

    command -v brightnessctl >/dev/null 2>&1 || {
        notify "brightnessctl not found"
        return 1
    }

    before="$(current_percent)"
    [[ -n "$before" ]] || before=0

    brightnessctl set "$value" >/dev/null 2>&1 || {
        notify "Operation failed"
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
        notify "brightnessctl not found"
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

pick_slider() {
    local current value
    current="$(current_percent)"
    [[ -n "$current" ]] || current=0

    value="$(rofi_slider_pick "slider-brightness" "Brightness" "Main screen" "$current" 0 100 1 "brightness")"
    [[ -z "$value" ]] && return 0
    if command -v brightnessctl >/dev/null 2>&1; then
        brightnessctl set "${value}%" >/dev/null 2>&1 || true
        show_osd "$value"
    fi
}

case "${1:-menu}" in
    up) adjust_brightness up ;;
    down) adjust_brightness down ;;
    25 | 25%) set_brightness "25%" ;;
    50 | 50%) set_brightness "50%" ;;
    75 | 75%) set_brightness "75%" ;;
    100 | 100%) set_brightness "100%" ;;
    menu) pick_slider ;;
    *)
        printf 'Uso: %s [menu|up|down|25|50|75|100]\n' "$0" >&2
        exit 2
        ;;
esac
