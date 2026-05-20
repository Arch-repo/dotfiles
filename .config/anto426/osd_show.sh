#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OSD_PY="$SCRIPT_DIR/osd/osd.py"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}"
PID_FILE="$RUNTIME_DIR/anto426-osd.pid"
STATE_FILE="$RUNTIME_DIR/anto426-osd.state"

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
    local message="${4:-}"
    mkdir -p "$RUNTIME_DIR"
    printf '%s %s %s %s\n' "$kind" "$value" "$muted" "$message" >"$STATE_FILE"
}

start_daemon() {
    [[ -f "$OSD_PY" ]] || return 1
    command -v python3 >/dev/null 2>&1 || return 1

    ANTO426_OSD_STATE="$STATE_FILE" ANTO426_OSD_PID="$PID_FILE" \
        python3 "$OSD_PY" daemon >/dev/null 2>&1 &
    printf '%s\n' "$!" >"$PID_FILE"
}

osd_pid_running() {
    local pid="$1"
    local cmdline

    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null || return 1
    [[ -r "/proc/$pid/cmdline" ]] || return 1
    cmdline="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
    [[ "$cmdline" == *"$OSD_PY"* ]]
}

show_osd() {
    local kind="${1:-volume}"
    local value
    local muted="${3:-0}"
    local pid

    value="$(clamp_percent "${2:-0}")"
    write_state "$kind" "$value" "$muted"

    if [[ -f "$PID_FILE" ]]; then
        pid="$(cat "$PID_FILE" 2>/dev/null || true)"
        if osd_pid_running "$pid"; then
            kill -USR1 "$pid" 2>/dev/null && return 0
        fi
        rm -f "$PID_FILE"
    fi

    start_daemon
}

case "${1:-}" in
    volume | brightness | mic)
        show_osd "$1" "${2:-0}" "${3:-0}"
        ;;
    clipboard | toast)
        write_state "${1:-toast}" 0 0 "${2:-Copied to clipboard}"
        if [[ -f "$PID_FILE" ]]; then
            pid="$(cat "$PID_FILE" 2>/dev/null || true)"
            if osd_pid_running "$pid"; then
                kill -USR1 "$pid" 2>/dev/null && exit 0
            fi
            rm -f "$PID_FILE"
        fi
        start_daemon
        ;;
    hide)
        if [[ -f "$PID_FILE" ]]; then
            pid="$(cat "$PID_FILE" 2>/dev/null || true)"
            osd_pid_running "$pid" && kill "$pid" 2>/dev/null || true
        fi
        rm -f "$PID_FILE"
        ;;
    *)
        printf 'Uso: %s <volume|brightness|mic> <0-100> [muted] | clipboard [messaggio]\n' "$0" >&2
        exit 2
        ;;
esac
