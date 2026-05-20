#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

THEME="$HOME/.config/rofi/control_menu.rasi"
OSD_SCRIPT="$HOME/.config/anto426/osd_show.sh"

choice="$(cliphist list | rofi -dmenu -p "Clipboard" -theme "$THEME")"
[[ -n "$choice" ]] || exit 0

if printf '%s' "$choice" | cliphist decode | wl-copy; then
    [[ -x "$OSD_SCRIPT" ]] && "$OSD_SCRIPT" clipboard "Copied to clipboard"
fi
