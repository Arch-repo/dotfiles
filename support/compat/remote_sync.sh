#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
data_dir="${XDG_DATA_HOME:-$HOME/.local/share}/anto426"
config_file="$data_dir/sync.env"
calendar_dir="$data_dir/calendar"
ics_file="$calendar_dir/google.ics"
events_file="$calendar_dir/google_events.json"
reminders_file="$calendar_dir/google_reminders.sent"
sync_core="$script_dir/remote_sync_core"
lock_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-remote-sync.lock"
quiet="${ANTO426_SYNC_QUIET:-0}"

notify() {
    [[ "$quiet" == "1" ]] && return 0
    notify-send "anto426 sync" "$*" 2>/dev/null || true
}

ensure_config() {
    mkdir -p "$data_dir" "$calendar_dir"
    if [[ ! -f "$config_file" ]]; then
        cat >"$config_file" <<'EOF'
# Google Calendar: incolla l'indirizzo iCal segreto dalle impostazioni.
# export ANTO426_GCAL_ICS_URL='https://calendar.google.com/calendar/ical/.../basic.ics'
export ANTO426_GCAL_SYNC_PAST_DAYS=7
export ANTO426_GCAL_SYNC_FUTURE_DAYS=120
export ANTO426_SYNC_INTERVAL=900

# Fastfetch/neofetch: non modificato automaticamente dalla sincronizzazione.
export ANTO426_NEOFETCH_IMAGES=1
export ANTO426_NEOFETCH_AUTO_SYNC=0
EOF
    fi
}

load_config() {
    ensure_config
    # shellcheck disable=SC1090
    source "$config_file"
}

notify_due_events() {
    [[ -s "$events_file" && -x "$sync_core" ]] || return 0
    local lookback timezone
    lookback="${ANTO426_GCAL_REMINDER_LOOKBACK:-${ANTO426_SYNC_INTERVAL:-900}}"
    [[ "$lookback" =~ ^[0-9]+$ ]] || lookback=900
    timezone="${TZ:-Europe/Rome}"
    "$sync_core" due "$events_file" "$reminders_file" "$lookback" "$timezone" |
        while IFS=$'\t' read -r title body; do
            [[ -n "$title" ]] || continue
            notify-send -a "anto426 Calendar" -i "x-office-calendar" "$title" "$body" 2>/dev/null || true
        done
}

sync_calendar() {
    load_config
    local url="${ANTO426_GCAL_ICS_URL:-}"
    if [[ -z "$url" ]]; then
        notify "Google Calendar non configurato: $config_file"
        return 2
    fi
    [[ -x "$sync_core" ]] || {
        notify "remote_sync_core non compilato"
        return 1
    }

    local temporary past future timezone count
    temporary="$(mktemp)"
    trap 'rm -f "$temporary"' RETURN
    past="${ANTO426_GCAL_SYNC_PAST_DAYS:-7}"
    future="${ANTO426_GCAL_SYNC_FUTURE_DAYS:-120}"
    timezone="${TZ:-Europe/Rome}"

    curl --connect-timeout 10 --max-time 30 -fsSL "$url" -o "$temporary" || {
        notify "Download Google Calendar non riuscito"
        return 1
    }
    "$sync_core" sync-calendar "$temporary" "$events_file" "$past" "$future" "$timezone" || {
        notify "Conversione del calendario non riuscita"
        return 1
    }
    mv "$temporary" "$ics_file"
    trap - RETURN
    count="$(jq 'length' "$events_file" 2>/dev/null || printf '0')"
    notify "Google Calendar aggiornato: $count eventi"
    notify_due_events
}

run_daemon() {
    if ! mkdir "$lock_dir" 2>/dev/null; then
        local previous
        previous="$(cat "$lock_dir/pid" 2>/dev/null || true)"
        [[ -n "$previous" ]] && kill -0 "$previous" 2>/dev/null && return 0
        rm -f "$lock_dir/pid" 2>/dev/null || true
        rmdir "$lock_dir" 2>/dev/null || true
        mkdir "$lock_dir" 2>/dev/null || return 0
    fi
    printf '%s\n' "$$" >"$lock_dir/pid"
    trap 'rm -f "$lock_dir/pid" 2>/dev/null || true; rmdir "$lock_dir" 2>/dev/null || true' EXIT INT TERM
    load_config
    local interval="${ANTO426_SYNC_INTERVAL:-900}"
    [[ "$interval" =~ ^[0-9]+$ && "$interval" -ge 60 ]] || interval=900
    while true; do
        ANTO426_SYNC_QUIET=1 "$0" calendar >/dev/null 2>&1 || true
        sleep "$interval"
    done
}

case "${1:-calendar}" in
    init)
        ensure_config
        notify "Configurazione pronta: $config_file"
        ;;
    config)
        ensure_config
        xdg-open "$config_file" >/dev/null 2>&1 &
        ;;
    calendar|all)
        sync_calendar
        ;;
    daemon)
        run_daemon
        ;;
    *)
        printf 'Uso: %s [init|config|calendar|daemon]\n' "$0" >&2
        exit 2
        ;;
esac
