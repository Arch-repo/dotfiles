#!/usr/bin/env bash
set -uo pipefail

target="${1:-}"
value="${2:-}"

number() {
    local raw="${1:-}"
    raw="${raw//,/.}"
    if [[ "$raw" =~ ^-?[0-9]+([.][0-9]+)?$ ]]; then
        printf '%s' "$raw"
    else
        printf '0'
    fi
}

percent() {
    local raw
    raw="$(number "${1:-0}")"
    awk -v value="$raw" '
        BEGIN {
            v = int(value + 0.5)
            if (v < 0) v = 0
            if (v > 100) v = 100
            printf "%d", v
        }
    '
}

clamp_int() {
    local raw min max
    raw="$(number "${1:-0}")"
    min="${2:-0}"
    max="${3:-100}"
    awk -v value="$raw" -v min="$min" -v max="$max" '
        BEGIN {
            v = int(value + 0.5)
            low = int(min + 0)
            high = int(max + 0)
            if (v < low) v = low
            if (v > high) v = high
            printf "%d", v
        }
    '
}

case "$target" in
    output-volume)
        value="$(percent "$value")"
        command -v wpctl >/dev/null 2>&1 &&
            wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ "${value}%" >/dev/null 2>&1
        ;;
    input-volume | mic-volume)
        value="$(percent "$value")"
        command -v wpctl >/dev/null 2>&1 &&
            wpctl set-volume -l 1 @DEFAULT_AUDIO_SOURCE@ "${value}%" >/dev/null 2>&1
        ;;
    brightness)
        value="$(percent "$value")"
        command -v brightnessctl >/dev/null 2>&1 &&
            brightnessctl set "${value}%" >/dev/null 2>&1
        ;;
    fan-pwm)
        value="$(clamp_int "$value" 0 255)"
        "$HOME/.config/anto426/hardware_stats.sh" set-fan-pwm "$value" >/dev/null 2>&1
        ;;
    *)
        exit 2
        ;;
esac
