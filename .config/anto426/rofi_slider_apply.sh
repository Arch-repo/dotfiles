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

value="$(percent "$value")"

case "$target" in
    output-volume)
        command -v wpctl >/dev/null 2>&1 &&
            wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ "${value}%" >/dev/null 2>&1
        ;;
    input-volume | mic-volume)
        command -v wpctl >/dev/null 2>&1 &&
            wpctl set-volume -l 1 @DEFAULT_AUDIO_SOURCE@ "${value}%" >/dev/null 2>&1
        ;;
    brightness)
        command -v brightnessctl >/dev/null 2>&1 &&
            brightnessctl set "${value}%" >/dev/null 2>&1
        ;;
    *)
        exit 2
        ;;
esac
