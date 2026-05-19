#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

THEME="$HOME/.config/rofi/config.rasi"

rofi -show emoji -theme "$THEME"
