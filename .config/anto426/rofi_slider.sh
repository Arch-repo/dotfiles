#!/usr/bin/env bash
# Rofi slider panel helper (requires rofi built with -slider-name / -slider-value).
set -uo pipefail

SCRIPT_DIR="${ROFI_SLIDER_SCRIPT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)}"
THEME_SLIDER="${ROFI_SLIDER_THEME:-$HOME/.config/rofi/control_slider.rasi}"
ROFI_SLIDER_APPLY="${ROFI_SLIDER_APPLY:-$SCRIPT_DIR/rofi_slider_apply.sh}"

_rofi_slider_number() {
    local value="${1:-}"
    local fallback="${2:-0}"

    value="${value//,/.}"
    if [[ "$value" =~ ^-?[0-9]+([.][0-9]+)?$ ]]; then
        printf '%s' "$value"
    else
        printf '%s' "$fallback"
    fi
}

_rofi_slider_clamp() {
    local value min max step

    min="$(_rofi_slider_number "${2:-0}" 0)"
    max="$(_rofi_slider_number "${3:-100}" 100)"
    step="$(_rofi_slider_number "${4:-1}" 1)"
    value="$(_rofi_slider_number "${1:-$min}" "$min")"

    awk -v value="$value" -v min="$min" -v max="$max" -v step="$step" '
        BEGIN {
            v = value + 0
            low = min + 0
            high = max + 0
            s = step + 0
            if (low > high) {
                tmp = low
                low = high
                high = tmp
            }
            if (v < low) v = low
            if (v > high) v = high
            if (s > 0) {
                v = low + int(((v - low) / s) + 0.5) * s
                if (v < low) v = low
                if (v > high) v = high
            }
            if (v == int(v)) {
                printf "%d", v
            } else {
                printf "%.6g", v
            }
        }
    '
}

_rofi_slider_supports_live() {
    [[ "${ROFI_SLIDER_DISABLE_LIVE:-0}" == "1" ]] && return 1

    if [[ -n "${_ROFI_SLIDER_LIVE_SUPPORT:-}" ]]; then
        [[ "$_ROFI_SLIDER_LIVE_SUPPORT" == "1" ]]
        return
    fi

    if rofi -help 2>&1 | grep -Fq -- "-slider-change-command"; then
        _ROFI_SLIDER_LIVE_SUPPORT=1
    else
        _ROFI_SLIDER_LIVE_SUPPORT=0
    fi

    [[ "$_ROFI_SLIDER_LIVE_SUPPORT" == "1" ]]
}

rofi_slider_pick() {
    local slider_name="$1"
    local prompt="$2"
    local message="${3:-}"
    local current="${4:-0}"
    local min="${5:-0}"
    local max="${6:-100}"
    local step="${7:-1}"
    local live_target="${8:-}"
    local panel_children="${9:-control-slider-panel}"

    [[ -n "$slider_name" ]] || return 1
    min="$(_rofi_slider_number "$min" 0)"
    max="$(_rofi_slider_number "$max" 100)"
    step="$(_rofi_slider_number "$step" 1)"
    current="$(_rofi_slider_clamp "$current" "$min" "$max" "$step")"

    if [[ "$message" != *"{value}"* ]]; then
        if [[ -n "$message" ]]; then
            message="$(printf '%b' "$message")"$'\n'"<span weight='bold'>{value}%</span>"
        else
            message="<span weight='bold'>{value}%</span>"
        fi
    else
        message="$(printf '%b' "$message")"
    fi

    local theme_str
    if [[ -n "$message" ]]; then
        theme_str="mainbox { children: [message, ${panel_children}]; } ${panel_children} { children: [${slider_name}]; } ${slider_name} { min: ${min}; max: ${max}; step: ${step}; value: ${current}; }"
    else
        theme_str="mainbox { children: [${panel_children}]; } ${panel_children} { children: [${slider_name}]; } ${slider_name} { min: ${min}; max: ${max}; step: ${step}; value: ${current}; }"
    fi

    local rofi_args=(
        -dmenu
        -theme "$THEME_SLIDER"
        -theme-str "$theme_str"
        -slider-name "$slider_name"
        -slider-value "$current"
        -p "$prompt"
        -mesg "$message"
    )

    if [[ -n "$live_target" && -f "$ROFI_SLIDER_APPLY" ]] &&
        _rofi_slider_supports_live; then
        local apply_command
        printf -v apply_command '%q %q %q {value}' bash "$ROFI_SLIDER_APPLY" "$live_target"
        rofi_args+=(
            -slider-change-command "$apply_command"
        )
    fi

    printf '' | rofi "${rofi_args[@]}"
}
