#!/usr/bin/env bash
[[ -n "${ANTO426_ROFI_LIB_LOADED:-}" ]] && return 0
ANTO426_ROFI_LIB_LOADED=1
export PATH="$HOME/.config/anto426/bin:$PATH"

ANTO426_THEME_CONTROL="${ANTO426_THEME_CONTROL:-$HOME/.config/rofi/control_menu.rasi}"
ANTO426_THEME_LAUNCHER="${ANTO426_THEME_LAUNCHER:-$HOME/.config/rofi/config.rasi}"

anto426_close_rofi() {
    pkill -x rofi 2>/dev/null || true
}

anto426_rofi_dmenu() {
    local prompt="$1"
    local theme

    shift || return 1
    theme="${1:-$ANTO426_THEME_CONTROL}"
    if (($# > 0)); then
        shift
    fi

    rofi -dmenu -i -matching fuzzy -p "$prompt" -theme "$theme" "$@"
}
