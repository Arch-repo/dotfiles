#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

THEME="$HOME/.config/rofi/config.rasi"

rofi -show drun \
    -show-icons \
    -icon-theme "WhiteSur-dark" \
    -application-fallback-icon "application-default-icon" \
    -p "Apps" \
    -matching fuzzy \
    -sort \
    -sorting-method fzf \
    -theme "$THEME"
