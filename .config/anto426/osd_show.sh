#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSD_BIN="$SCRIPT_DIR/osd/osd"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}"
PID_FILE="$RUNTIME_DIR/anto426-osd.pid"
STATE_FILE="$RUNTIME_DIR/anto426-osd.state"
LOCK_DIR="$RUNTIME_DIR/anto426-osd.lock"

clamp_percent() {
    local raw="${1:-0}"
    awk -v value="$raw" 'BEGIN {
        v = int(value + 0.5)
        if (v < 0) v = 0
        if (v > 100) v = 100
        print v
    }'
}

write_state() {
    local kind="$1"
    local value="$2"
    local muted="${3:-0}"
    mkdir -p "$RUNTIME_DIR"
    printf '%s %s %s\n' "$kind" "$value" "$muted" >"$STATE_FILE"
}

start_daemon() {
    [[ -x "$OSD_BIN" ]] || return 1

    GDK_BACKEND=wayland ANTO426_OSD_STATE="$STATE_FILE" ANTO426_OSD_PID="$PID_FILE" \
        "$OSD_BIN" daemon >/dev/null 2>&1 &
    printf '%s\n' "$!" >"$PID_FILE"
}

acquire_lock() {
    local i lock_age

    for ((i = 0; i < 24; i++)); do
        if mkdir "$LOCK_DIR" 2>/dev/null; then
            return 0
        fi

        lock_age="$(($(date +%s) - $(stat -c %Y "$LOCK_DIR" 2>/dev/null || date +%s)))"
        if (( lock_age > 2 )); then
            rmdir "$LOCK_DIR" 2>/dev/null || rm -rf "$LOCK_DIR" 2>/dev/null || true
            continue
        fi

        sleep 0.025
    done

    return 1
}

release_lock() {
    rmdir "$LOCK_DIR" 2>/dev/null || true
}

osd_pid_running() {
    local pid="$1"
    local cmdline

    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null || return 1
    [[ -r "/proc/$pid/cmdline" ]] || return 1
    cmdline="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
    [[ "$cmdline" == *"$OSD_BIN"* ]]
}

show_osd() {
    local kind="${1:-volume}"
    local value
    local muted="${3:-0}"
    local pid

    value="$(clamp_percent "${2:-0}")"
    write_state "$kind" "$value" "$muted"

    acquire_lock || {
        if [[ -f "$PID_FILE" ]]; then
            pid="$(cat "$PID_FILE" 2>/dev/null || true)"
            osd_pid_running "$pid" && kill -USR1 "$pid" 2>/dev/null && return 0
        fi
        return 1
    }

    if [[ -f "$PID_FILE" ]]; then
        pid="$(cat "$PID_FILE" 2>/dev/null || true)"
        if osd_pid_running "$pid"; then
            if kill -USR1 "$pid" 2>/dev/null; then
                release_lock
                return 0
            fi
        fi
        rm -f "$PID_FILE"
    fi

    if start_daemon; then
        release_lock
        return 0
    fi

    release_lock
    return 1
}

case "${1:-}" in
    volume | brightness | mic)
        show_osd "$1" "${2:-0}" "${3:-0}"
        ;;
    hide)
        if [[ -f "$PID_FILE" ]]; then
            pid="$(cat "$PID_FILE" 2>/dev/null || true)"
            osd_pid_running "$pid" && kill "$pid" 2>/dev/null || true
        fi
        rm -f "$PID_FILE"
        ;;
    *)
        printf 'Uso: %s <volume|brightness|mic> <0-100> [muted]\n' "$0" >&2
        exit 2
        ;;
esac
