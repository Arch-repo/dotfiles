#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"
padding="${ANTO426_FLOATING_PADDING:-32}"
move_step="${ANTO426_FLOATING_MOVE_STEP:-80}"

notify() {
    notify-send "Floating" "$*" 2>/dev/null || true
}

require_stack() {
    command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1
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

has_active_window() {
    active_json | jq -e '(.address // "") != ""' >/dev/null 2>&1
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
    local current_w current_h new_w new_h step=80 dw=0 dh=0

    case "$direction" in
        left | l | right | r | up | u | down | d) ;;
        *) return 1 ;;
    esac

    read -r current_w current_h < <(window_size)
    [[ -n "${current_w:-}" && -n "${current_h:-}" ]] || return 1

    new_w="$current_w"
    new_h="$current_h"

    case "$direction" in
        left | l)
            new_w=$((current_w - step))
            dw=$((-step))
            ;;
        right | r)
            new_w=$((current_w + step))
            dw="$step"
            ;;
        up | u)
            new_h=$((current_h - step))
            dh=$((-step))
            ;;
        down | d)
            new_h=$((current_h + step))
            dh="$step"
            ;;
    esac

    ((new_w < 240 || new_h < 160)) && return 0

    dispatch resizeactive "$dw $dh" || resize_exact "$new_w" "$new_h"
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
                "No active window"
            else
                "Window: \(.class) - \(.title)\nFloating: \(.floating)  Pinned: \(.pinned)"
            end
        ' 2>/dev/null ||
        printf 'No active window'
}

pick_corner() {
    local choice
    choice="$(
        printf '%s\n' \
            "󰘕 SELECT CORNER" \
            " ├─ 󰁝 Top left" \
            " ├─ 󰁔 Top right" \
            " ├─ 󰁅 Bottom left" \
            " ├─ 󰁜 Bottom right" \
            " └─ 󰌍 Back" |
            rofi -dmenu -i -matching fuzzy -p "Corner" -theme "$THEME"
    )"
    [[ -z "$choice" ]] && return 0
    if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
        pick_corner
        return 0
    fi
    printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//'
}

menu() {
    local choice corner

    require_stack || {
        notify "Hyprland or jq not available"
        return 1
    }

    if ! has_active_window; then
        choice="$(
            printf '%s\n' \
                "󰌍 WINDOW ABSENT" \
                " └─ 󰌍 Back" |
                rofi -dmenu -i -matching fuzzy \
                    -p "Floating Manager" \
                    -mesg "No active window" \
                    -theme "$THEME"
        )"
        [[ "$choice" == *"Indietro"* || "$choice" == *"Back"* ]] && go_back
        return 0
    fi

    choice="$(
        {
            printf '󰱒 WINDOW POSITION\n'
            printf ' ├─ 󰱒 Toggle floating\n'
            printf ' ├─ 󰁌 Center\n'
            printf ' └─ 󰘕 Move to corners\n'
            
            printf '\n󰾅 SIZE PRESETS\n'
            printf ' ├─ 󰾆 Compact 45%%\n'
            printf ' ├─ 󰾅 Comfortable 62%%\n'
            printf ' └─ 󰓡 Large 78%%\n'
            
            printf '\n󰐃 ACTIONS AND STATUS\n'
            printf ' ├─ 󰐃 Pin / unpin\n'
            printf ' ├─ 󰓌 Bring to front\n'
            printf ' ├─ 󰅖 Reset: tiled + unpin\n'
            printf ' └─ 󰅙 Close window\n'
            
            printf '\n󰌍 NAVIGATION\n'
            printf ' └─ 󰌍 Back\n'
        } |
            rofi -dmenu -i -matching fuzzy \
                -p "Floating Manager" \
                -mesg "$(active_summary)" \
                -theme "$THEME"
    )"

    [[ -z "$choice" ]] && return 0
    if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
        menu
        return 0
    fi
    local clean_choice
    clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

    case "$clean_choice" in
        *"Toggle"*) toggle_floating ;;
        *"Center"* | *"Centra"*) center_window ;;
        *"Compact"* | *"Compatta"*) resize_preset small ;;
        *"Comfortable"* | *"Comoda"*) resize_preset medium ;;
        *"Large"* | *"Grande"*) resize_preset large ;;
        *"Pin"*) dispatch pin ;;
        *"Bring to front"* | *"Porta sopra"*) dispatch alterzorder top ;;
        *"corners"* | *"angoli"*)
            corner="$(pick_corner)"
            case "$corner" in
                *"Top left"* | *"Alto sinistra"*) move_corner tl ;;
                *"Top right"* | *"Alto destra"*) move_corner tr ;;
                *"Bottom left"* | *"Basso sinistra"*) move_corner bl ;;
                *"Bottom right"* | *"Basso destra"*) move_corner br ;;
                *"Back"* | *"Indietro"*) menu ;;
            esac
            ;;
        *"Reset"*) reset_window ;;
        *"Close"* | *"Chiudi"*) dispatch killactive ;;
        *"Back"* | *"Indietro"*) go_back ;;
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
