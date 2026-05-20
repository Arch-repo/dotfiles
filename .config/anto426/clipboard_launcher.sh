#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

THEME="$HOME/.config/rofi/control_menu.rasi"

choice="$(cliphist list | rofi -dmenu -p "Clipboard" -theme "$THEME")"
[[ -n "$choice" ]] || exit 0

printf '%s' "$choice" | cliphist decode | wl-copy
