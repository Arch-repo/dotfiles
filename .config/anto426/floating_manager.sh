#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"
padding="${ANTO426_FLOATING_PADDING:-32}"
move_step="${ANTO426_FLOATING_MOVE_STEP:-80}"

notify() {
    notify-send "Floating" "$*" 2>/dev/null || true
}

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    return 0
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
    active_flag floating || dispatch setfloating active
}

toggle_floating() {
    if active_flag floating; then
        dispatch settiled active
    else
        dispatch setfloating active
        dispatch alterzorder top
    fi
}

move_exact() {
    dispatch moveactive "exact $1 $2"
}

resize_exact() {
    dispatch resizeactive "exact $1 $2"
}

move_or_tile() {
    local direction="$1"
    local dx=0 dy=0 tiled_direction

    case "$direction" in
        left | l) dx=$((-move_step)); tiled_direction="l" ;;
        right | r) dx="$move_step"; tiled_direction="r" ;;
        up | u) dy=$((-move_step)); tiled_direction="u" ;;
        down | d) dy="$move_step"; tiled_direction="d" ;;
        *) return 1 ;;
    esac

    if active_flag floating; then
        dispatch moveactive "$dx $dy"
    else
        dispatch movewindow "$tiled_direction"
    fi
}

resize_direction() {
    local direction="$1"
    local current_w current_h new_w new_h step=80

    case "$direction" in
        left | l | right | r | up | u | down | d) ;;
        *) return 1 ;;
    esac

    read -r current_w current_h < <(window_size)
    [[ -n "${current_w:-}" && -n "${current_h:-}" ]] || return 1

    new_w="$current_w"
    new_h="$current_h"

    case "$direction" in
        left | l) new_w=$((current_w - step)) ;;
        right | r) new_w=$((current_w + step)) ;;
        up | u) new_h=$((current_h - step)) ;;
        down | d) new_h=$((current_h + step)) ;;
    esac

    ((new_w < 240)) && new_w=240
    ((new_h < 160)) && new_h=160

    resize_exact "$new_w" "$new_h"
}

center_window() {
    ensure_floating
    dispatch centerwindow 1 || dispatch centerwindow
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
        dispatch settiled active
    fi
}

active_summary() {
    active_json |
        jq -r '
            if (.class == null) then
                "Nessuna finestra attiva"
            else
                "Finestra: \(.class) - \(.title)\nFloating: \(.floating)  Pinned: \(.pinned)"
            end
        ' 2>/dev/null ||
        printf 'Nessuna finestra attiva'
}

pick_corner() {
    printf '%s\n' \
        "󰁝 Alto sinistra" \
        "󰁔 Alto destra" \
        "󰁅 Basso sinistra" \
        "󰁜 Basso destra" \
        "󰌍 Indietro" |
        rofi -dmenu -i -matching fuzzy -p "Angolo" -theme "$THEME"
}

menu() {
    local choice corner

    choice="$(
        printf '%s\n' \
            "󰱒 Toggle floating" \
            "󰁌 Centra" \
            "󰾆 Compatta 45%" \
            "󰾅 Comoda 62%" \
            "󰓡 Grande 78%" \
            "󰐃 Pin / unpin" \
            "󰓌 Porta sopra" \
            "󰘕 Sposta agli angoli" \
            "󰅖 Reset: tiled + unpin" \
            "󰅙 Chiudi finestra" \
            "󰌍 Indietro" |
            rofi -dmenu -i -matching fuzzy \
                -p "Floating Manager" \
                -mesg "$(active_summary)" \
                -theme "$THEME"
    )"

    case "$choice" in
        *"Toggle"*) toggle_floating ;;
        *"Centra"*) center_window ;;
        *"Compatta"*) resize_preset small ;;
        *"Comoda"*) resize_preset medium ;;
        *"Grande"*) resize_preset large ;;
        *"Pin"*) dispatch pin ;;
        *"Porta sopra"*) dispatch alterzorder top ;;
        *"angoli"*)
            corner="$(pick_corner)"
            case "$corner" in
                *"Alto sinistra"*) move_corner tl ;;
                *"Alto destra"*) move_corner tr ;;
                *"Basso sinistra"*) move_corner bl ;;
                *"Basso destra"*) move_corner br ;;
                *"Indietro"*) go_back ;;
            esac
            ;;
        *"Reset"*) reset_window ;;
        *"Chiudi"*) dispatch killactive ;;
        *"Indietro"*) go_back ;;
    esac
}

case "${1:-menu}" in
    menu) menu ;;
    toggle) toggle_floating ;;
    center) center_window ;;
    small) resize_preset small ;;
    medium) resize_preset medium ;;
    large) resize_preset large ;;
    move-left) move_or_tile left ;;
    move-right) move_or_tile right ;;
    move-up) move_or_tile up ;;
    move-down) move_or_tile down ;;
    resize-left) resize_direction left ;;
    resize-right) resize_direction right ;;
    resize-up) resize_direction up ;;
    resize-down) resize_direction down ;;
    pin) dispatch pin ;;
    top) dispatch alterzorder top ;;
    reset) reset_window ;;
    close) dispatch killactive ;;
    corner-tl) move_corner tl ;;
    corner-tr) move_corner tr ;;
    corner-bl) move_corner bl ;;
    corner-br) move_corner br ;;
    *)
        printf 'Uso: %s [menu|toggle|center|small|medium|large|move-left|move-right|move-up|move-down|resize-left|resize-right|resize-up|resize-down|pin|top|reset|close|corner-tl|corner-tr|corner-bl|corner-br]\n' "$0" >&2
        exit 2
        ;;
esac
