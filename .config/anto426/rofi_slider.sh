#!/usr/bin/env bash
# Rofi slider panel helper (requires rofi built with -slider-name / -slider-value).
set -uo pipefail

THEME_SLIDER="${ROFI_SLIDER_THEME:-$HOME/.config/rofi/control_slider.rasi}"

rofi_slider_pick() {
    local slider_name="$1"
    local prompt="$2"
    local message="${3:-}"
    local current="${4:-0}"
    local panel_children="${5:-slider-panel}"

    [[ -n "$slider_name" ]] || return 1
    current="${current//[^0-9]/}"
    [[ -n "$current" ]] || current=0
    ((current < 0)) && current=0
    ((current > 100)) && current=100
    message="$(printf '%b' "$message")"

    printf '' |
        rofi -dmenu \
            -theme "$THEME_SLIDER" \
            -theme-str "mainbox { children: [message, ${panel_children}, inputbar]; } ${panel_children} { children: [${slider_name}]; } ${slider_name} { value: ${current}; }" \
            -slider-name "$slider_name" \
            -slider-value "$current" \
            -p "$prompt" \
            -mesg "$message"
}
