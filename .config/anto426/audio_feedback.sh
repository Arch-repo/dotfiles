#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSD_SCRIPT="$SCRIPT_DIR/osd_show.sh"

target_sink="@DEFAULT_AUDIO_SINK@"
target_source="@DEFAULT_AUDIO_SOURCE@"

show_osd() {
    [[ -x "$OSD_SCRIPT" ]] || return 0
    "$OSD_SCRIPT" "$@"
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

muted_flag() {
    if muted_state "$1"; then
        printf '1'
    else
        printf '0'
    fi
}

change_volume() {
    local direction="$1"
    local before after muted

    before="$(volume_percent "$target_sink")"
    [[ -n "$before" ]] || before=0

    case "$direction" in
        up)
            if ((before >= 100)); then
                show_osd volume "$before" "$(muted_flag "$target_sink")"
                return 0
            fi
            wpctl set-volume -l 1 "$target_sink" 5%+
            ;;
        down)
            if ((before <= 0)); then
                show_osd volume "$before" "$(muted_flag "$target_sink")"
                return 0
            fi
            wpctl set-volume "$target_sink" 5%-
            ;;
        *)
            return 1
            ;;
    esac

    after="$(volume_percent "$target_sink")"
    [[ -n "$after" ]] || after="$before"
    muted="$(muted_flag "$target_sink")"
    [[ "$after" == "$before" ]] && {
        show_osd volume "$after" "$muted"
        return 0
    }

    show_osd volume "$after" "$muted"
}

case "${1:-}" in
    volume-up)
        change_volume up
        ;;
    volume-down)
        change_volume down
        ;;
    mute)
        wpctl set-mute "$target_sink" toggle
        if muted_state "$target_sink"; then
            show_osd volume 0 1
        else
            show_osd volume "$(volume_percent "$target_sink")" 0
        fi
        ;;
    mic-mute)
        wpctl set-mute "$target_source" toggle
        if muted_state "$target_source"; then
            show_osd mic 0 1
        else
            show_osd mic "$(volume_percent "$target_source")" 0
        fi
        ;;
    *)
        printf 'Uso: %s [volume-up|volume-down|mute|mic-mute]\n' "$0" >&2
        exit 2
        ;;
esac
