#!/usr/bin/env bash
set -uo pipefail

target_sink="@DEFAULT_AUDIO_SINK@"
target_source="@DEFAULT_AUDIO_SOURCE@"

notify() {
    notify-send "$1" "$2" ${3:+-h "int:value:$3"} 2>/dev/null || true
}

volume_percent() {
    wpctl get-volume "$1" 2>/dev/null | awk '
        /^Volume:/ {
            value = int(($2 * 100) + 0.5)
            if (value < 0) value = 0
            if (value > 100) value = 100
            print value
            exit
        }
    '
}

muted_state() {
    wpctl get-volume "$1" 2>/dev/null | grep -q '\[MUTED\]'
}

case "${1:-}" in
    volume-up)
        wpctl set-volume -l 1 "$target_sink" 5%+
        value="$(volume_percent "$target_sink")"
        notify "Volume" "${value:-?}%" "${value:-}"
        ;;
    volume-down)
        wpctl set-volume "$target_sink" 5%-
        value="$(volume_percent "$target_sink")"
        notify "Volume" "${value:-?}%" "${value:-}"
        ;;
    mute)
        wpctl set-mute "$target_sink" toggle
        if muted_state "$target_sink"; then
            notify "Volume" "Muto"
        else
            value="$(volume_percent "$target_sink")"
            notify "Volume" "${value:-?}%" "${value:-}"
        fi
        ;;
    mic-mute)
        wpctl set-mute "$target_source" toggle
        if muted_state "$target_source"; then
            notify "Microfono" "Muto"
        else
            value="$(volume_percent "$target_source")"
            notify "Microfono" "${value:-?}%" "${value:-}"
        fi
        ;;
    *)
        printf 'Uso: %s [volume-up|volume-down|mute|mic-mute]\n' "$0" >&2
        exit 2
        ;;
esac
