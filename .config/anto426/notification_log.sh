#!/usr/bin/env bash
set -uo pipefail

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anto426/notifications"
HISTORY_FILE="$DATA_DIR/history.tsv"
LOCK_DIR="${XDG_RUNTIME_DIR:-/tmp}/anto426-notification-log.lock"
MAX_HISTORY=80

if ! mkdir "$LOCK_DIR" 2>/dev/null; then
    exit 0
fi

cleanup() {
    rmdir "$LOCK_DIR" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

mkdir -p "$DATA_DIR"
touch "$HISTORY_FILE"

append_notification() {
    local app="$1"
    local summary="$2"
    local body="$3"
    local tmp

    app="${app//$'\t'/ }"
    summary="${summary//$'\t'/ }"
    body="${body//$'\t'/ }"

    printf '%s\t%s\t%s\t%s\n' "$(date +%s)" "$app" "$summary" "$body" >> "$HISTORY_FILE"

    tmp="$(mktemp)"
    tail -n "$MAX_HISTORY" "$HISTORY_FILE" > "$tmp" &&
        mv "$tmp" "$HISTORY_FILE"
}

dbus-monitor "type='method_call',interface='org.freedesktop.Notifications',member='Notify'" |
    awk '
        /member=Notify/ {
            active = 1
            string_index = 0
            app = ""
            summary = ""
            body = ""
            next
        }

        active && /^[[:space:]]+string / {
            value = $0
            sub(/^[[:space:]]*string "/, "", value)
            sub(/"$/, "", value)

            string_index++
            if (string_index == 1) app = value
            else if (string_index == 3) summary = value
            else if (string_index == 4) {
                body = value
                printf "%s\t%s\t%s\n", app, summary, body
                fflush()
                active = 0
            }
        }
    ' |
    while IFS=$'\t' read -r app summary body; do
        [[ -n "$summary$body" ]] && append_notification "$app" "$summary" "$body"
    done
