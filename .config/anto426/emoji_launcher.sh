#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

THEME="$HOME/.config/rofi/control_menu.rasi"

rofi -show emoji -theme "$THEME"
