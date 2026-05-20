#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"
padding="${ANTO426_FLOATING_PADDING:-32}"

notify() {
    notify-send "Floating" "$*" 2>/dev/null || true
}

active_json() {
    hyprctl activewindow -j 2>/dev/null
}

active_flag() {
    local field="$1"
    active_json | jq -er ".$field == true" >/dev/null 2>&1
}

monitor_geometry() {
    hyprctl monitors -j 2>/dev/null |
        jq -r '.[] | select(.focused == true) | "\(.x) \(.y) \((.width / .scale) | floor) \((.height / .scale) | floor)"' |
        sed -n '1p'
}

window_size() {
    active_json | jq -r 'select(.size != null) | "\(.size[0]) \(.size[1])"' | sed -n '1p'
}

dispatch() {
    hyprctl dispatch "$@" >/dev/null 2>&1
}

ensure_floating() {
    active_flag floating || dispatch togglefloating
}

move_exact() {
    dispatch movewindowpixel "exact $1 $2,activewindow"
}

resize_exact() {
    dispatch resizewindowpixel "exact $1 $2,activewindow"
}

center_window() {
    local mx my mw mh ww wh x y

    ensure_floating
    read -r mx my mw mh < <(monitor_geometry)
    read -r ww wh < <(window_size)
    [[ -n "${mw:-}" && -n "${ww:-}" ]] || return 1

    x=$((mx + (mw - ww) / 2))
    y=$((my + (mh - wh) / 2))
    ((x < mx + padding)) && x=$((mx + padding))
    ((y < my + padding)) && y=$((my + padding))
    move_exact "$x" "$y"
}

resize_preset() {
    local preset="$1"
    local mx my mw mh w h min_w min_h

    ensure_floating
    read -r mx my mw mh < <(monitor_geometry)
    [[ -n "${mw:-}" ]] || return 1

    case "$preset" in
        small)
            w=$((mw * 45 / 100)); h=$((mh * 45 / 100)); min_w=640; min_h=420
            ;;
        medium)
            w=$((mw * 62 / 100)); h=$((mh * 62 / 100)); min_w=860; min_h=560
            ;;
        large)
            w=$((mw * 78 / 100)); h=$((mh * 78 / 100)); min_w=1100; min_h=720
            ;;
        *)
            return 1
            ;;
    esac

    ((w < min_w)) && w=$min_w
    ((h < min_h)) && h=$min_h
    ((w > mw - padding * 2)) && w=$((mw - padding * 2))
    ((h > mh - padding * 2)) && h=$((mh - padding * 2))

    resize_exact "$w" "$h"
    sleep 0.05
    center_window
}

move_corner() {
    local corner="$1"
    local mx my mw mh ww wh x y

    ensure_floating
    read -r mx my mw mh < <(monitor_geometry)
    read -r ww wh < <(window_size)
    [[ -n "${mw:-}" && -n "${ww:-}" ]] || return 1

    case "$corner" in
        tl) x=$((mx + padding)); y=$((my + padding)) ;;
        tr) x=$((mx + mw - ww - padding)); y=$((my + padding)) ;;
        bl) x=$((mx + padding)); y=$((my + mh - wh - padding)) ;;
        br) x=$((mx + mw - ww - padding)); y=$((my + mh - wh - padding)) ;;
        *) return 1 ;;
    esac

    move_exact "$x" "$y"
}

reset_window() {
    active_flag pinned && dispatch pin

    if active_flag floating; then
        dispatch togglefloating
    fi
}

pick_corner() {
    printf '%s\n' \
        "󰁝 Alto sinistra" \
        "󰁔 Alto destra" \
        "󰁅 Basso sinistra" \
        "󰁜 Basso destra" |
        rofi -dmenu -i -matching fuzzy -p "Angolo" -theme "$THEME"
}

menu() {
    local choice corner

    choice="$(
        printf '%s\n' \
            "󰱒 Toggle floating" \
            "󰁌 Centra finestra" \
            "󰾆 Resize small" \
            "󰾅 Resize medium" \
            "󰓡 Resize large" \
            "󰐃 Pin / unpin" \
            "󰓌 Porta sopra" \
            "󰘕 Sposta ad angolo" \
            "󰅖 Reset floating" \
            "󰅙 Chiudi finestra" |
            rofi -dmenu -i -matching fuzzy -p "Floating Manager" -theme "$THEME"
    )"

    case "$choice" in
        *"Toggle"*) dispatch togglefloating ;;
        *"Centra"*) center_window ;;
        *"small"*) resize_preset small ;;
        *"medium"*) resize_preset medium ;;
        *"large"*) resize_preset large ;;
        *"Pin"*) dispatch pin ;;
        *"Porta sopra"*) dispatch alterzorder top ;;
        *"angolo"*)
            corner="$(pick_corner)"
            case "$corner" in
                *"Alto sinistra"*) move_corner tl ;;
                *"Alto destra"*) move_corner tr ;;
                *"Basso sinistra"*) move_corner bl ;;
                *"Basso destra"*) move_corner br ;;
            esac
            ;;
        *"Reset"*) reset_window ;;
        *"Chiudi"*) dispatch killactive ;;
    esac
}

case "${1:-menu}" in
    menu) menu ;;
    toggle) dispatch togglefloating ;;
    center) center_window ;;
    small) resize_preset small ;;
    medium) resize_preset medium ;;
    large) resize_preset large ;;
    pin) dispatch pin ;;
    top) dispatch alterzorder top ;;
    reset) reset_window ;;
    close) dispatch killactive ;;
    corner-tl) move_corner tl ;;
    corner-tr) move_corner tr ;;
    corner-bl) move_corner bl ;;
    corner-br) move_corner br ;;
    *)
        printf 'Uso: %s [menu|toggle|center|small|medium|large|pin|top|reset|close|corner-tl|corner-tr|corner-bl|corner-br]\n' "$0" >&2
        exit 2
        ;;
esac
