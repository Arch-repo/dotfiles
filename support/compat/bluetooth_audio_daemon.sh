#!/usr/bin/env bash
set -uo pipefail

source "$HOME/.config/anto426/lib/bluetooth_audio.sh"

state_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426"
state_file="$state_dir/bluetooth-audio.connected"
lock_dir="$state_dir/bluetooth-audio.lock"
log_file="${XDG_STATE_HOME:-$HOME/.local/state}/anto426/bluetooth-audio.log"

mkdir -p "$state_dir" "$(dirname "$log_file")"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

notify_bt_audio() {
    notify-send "Bluetooth audio" "$*" 2>/dev/null || true
}

cleanup() {
    rm -rf "$lock_dir" 2>/dev/null || true
}

connected_audio_macs() {
    bt_audio_rows | awk -F'\t' '$3 == "yes" {print $2}'
}

fallback_sink() {
    pactl list sinks short 2>/dev/null | awk '$2 !~ /^bluez_output\./ {print $2; exit}'
}

current_default_sink() {
    pactl info 2>/dev/null | awk -F': ' '/Default Sink:/ {print $2; exit}'
}

default_sink_belongs_to_mac() {
    local mac="$1"
    local safe default_sink
    safe="$(bt_audio_mac_safe "$mac")"
    default_sink="$(current_default_sink)"
    [[ "$default_sink" == bluez_output."$safe"* ]]
}

remember_connected() {
    connected_audio_macs >"$state_file"
}

was_connected() {
    local mac="$1"
    [[ -f "$state_file" ]] && grep -Fxq "$mac" "$state_file"
}

auto_apply_connected() {
    local mac name
    connected_audio_macs | while read -r mac; do
        [[ -z "$mac" ]] && continue
        was_connected "$mac" && continue

        name="$(bluetoothctl info "$mac" 2>/dev/null | awk -F': ' '/Alias:/ {print $2; exit}')"
        [[ -z "$name" ]] && name="$mac"

        if bt_audio_use "$mac" a2dp; then
            log "auto-selected $name ($mac) as Hi-Fi output"
            notify_bt_audio "$name impostate come output Hi-Fi"
        else
            log "failed to auto-select $name ($mac)"
        fi
    done
}

auto_apply_disconnected() {
    local previous current mac sink
    [[ -f "$state_file" ]] || return 0
    previous="$(cat "$state_file")"
    current="$(connected_audio_macs)"

    while read -r mac; do
        [[ -z "$mac" ]] && continue
        printf '%s\n' "$current" | grep -Fxq "$mac" && continue

        if default_sink_belongs_to_mac "$mac"; then
            sink="$(fallback_sink)"
            if [[ -n "$sink" ]]; then
                pactl set-default-sink "$sink" >/dev/null 2>&1 || true
                log "restored fallback sink after $mac disconnected: $sink"
            fi
        fi
    done <<< "$previous"
}

tick() {
    [[ "${ANTO426_BT_AUDIO_AUTOSWITCH:-1}" == "1" ]] || return 0
    command -v bluetoothctl >/dev/null 2>&1 || return 0
    command -v pactl >/dev/null 2>&1 || return 0

    auto_apply_disconnected
    auto_apply_connected
    remember_connected
}

daemon() {
    if ! mkdir "$lock_dir" 2>/dev/null; then
        local old_pid
        old_pid="$(cat "$lock_dir/pid" 2>/dev/null || true)"
        if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
            exit 0
        fi
        rm -rf "$lock_dir"
        mkdir "$lock_dir" 2>/dev/null || exit 0
    fi
    printf '%s\n' "$$" >"$lock_dir/pid"
    trap cleanup EXIT
    trap 'cleanup; exit 0' INT TERM

    log "daemon started"
    while true; do
        tick
        sleep 4
    done
}

case "${1:-daemon}" in
    daemon) daemon ;;
    once) tick ;;
    status) bt_audio_connected_summary ;;
    *)
        printf 'Usage: %s [daemon|once|status]\n' "$0" >&2
        exit 2
        ;;
esac
