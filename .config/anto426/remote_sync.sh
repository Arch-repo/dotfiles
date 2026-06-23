#!/usr/bin/env bash
set -uo pipefail
export PATH="$HOME/.config/anto426/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anto426"
CONFIG_FILE="$DATA_DIR/sync.env"
CALENDAR_DIR="$DATA_DIR/calendar"
GCAL_ICS_FILE="$CALENDAR_DIR/google.ics"
GCAL_EVENTS_FILE="$CALENDAR_DIR/google_events.json"
GCAL_REMINDER_STATE_FILE="$CALENDAR_DIR/google_reminders.sent"
REMOTE_SYNC_CORE="$SCRIPT_DIR/remote_sync_core"
THEME_SETUP="$HOME/.config/rofi/control_setup.rasi"
DAEMON_LOCK_DIR="${XDG_RUNTIME_DIR:-/tmp}/anto426-remote-sync.lock"

quiet="${ANTO426_SYNC_QUIET:-0}"

notify() {
    [[ "$quiet" == "1" ]] && return 0
    notify-send "anto426 sync" "$*" 2>/dev/null || true
}

rofi_pick_msg() {
    local prompt="$1"
    local message="$2"

    message="$(printf '%b' "$message")"
    rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -mesg "$message" -theme "$THEME_SETUP"
}

rofi_input() {
    local prompt="$1"
    local value="${2:-}"
    local message="${3:-}"

    message="$(printf '%b' "$message")"
    printf '%s' "$value" |
        rofi -dmenu -p "$prompt" -mesg "$message" -theme "$THEME_SETUP"
}

shell_quote() {
    local quoted
    printf -v quoted '%q' "$1"
    printf '%s' "$quoted"
}

ensure_neofetch_config() {
    if ! grep -q '^export ANTO426_NEOFETCH_IMAGES=' "$CONFIG_FILE" 2>/dev/null; then
        cat >> "$CONFIG_FILE" <<'EOF'

# Fastfetch/neofetch terminal logo behavior.
# 1 = enable images from ~/Pictures/neofetch.
export ANTO426_NEOFETCH_IMAGES=1
EOF
    fi

    if grep -q '^export ANTO426_NEOFETCH_AUTO_SYNC=' "$CONFIG_FILE" 2>/dev/null; then
        sed -i 's/^export ANTO426_NEOFETCH_AUTO_SYNC=.*/export ANTO426_NEOFETCH_AUTO_SYNC=0/' "$CONFIG_FILE"
    else
        cat >> "$CONFIG_FILE" <<'EOF'
# 0 = never repopulate ~/Pictures/neofetch from sync scripts.
export ANTO426_NEOFETCH_AUTO_SYNC=0
EOF
    fi
}

ensure_config() {
    mkdir -p "$DATA_DIR" "$CALENDAR_DIR"

    if [[ ! -f "$CONFIG_FILE" ]]; then
        cat > "$CONFIG_FILE" <<'EOF'
# Local Configuration (Independent from Codex)
#
# Google Calendar:
# 1. Open Google Calendar in your web browser.
# 2. Calendar Settings -> Integrate Calendar.
# 3. Copy the "Secret address in iCal format" and paste it below.
# export ANTO426_GCAL_ICS_URL='https://calendar.google.com/calendar/ical/.../basic.ics'
#
# Optional parameters:
# export ANTO426_GCAL_SYNC_PAST_DAYS=7
# export ANTO426_GCAL_SYNC_FUTURE_DAYS=120
# export ANTO426_SYNC_INTERVAL=900
EOF
    fi

    ensure_neofetch_config
}

load_config() {
    ensure_config
    # shellcheck disable=SC1090
    source "$CONFIG_FILE"
}

notify_calendar_due() {
    [[ -s "$GCAL_EVENTS_FILE" ]] || return 0
    [[ -x "$REMOTE_SYNC_CORE" ]] || return 0
    command -v notify-send >/dev/null 2>&1 || return 0

    local lookback timezone
    lookback="${ANTO426_GCAL_REMINDER_LOOKBACK:-${ANTO426_SYNC_INTERVAL:-900}}"
    [[ "$lookback" =~ ^[0-9]+$ ]] || lookback=900
    timezone="${TZ:-Europe/Rome}"

    "$REMOTE_SYNC_CORE" due "$GCAL_EVENTS_FILE" "$GCAL_REMINDER_STATE_FILE" "$lookback" "$timezone" |
    while IFS=$'\t' read -r title body; do
        [[ -n "$title" ]] || continue
        notify-send -a "anto426 Calendar" -i "x-office-calendar" "$title" "$body" 2>/dev/null || true
    done
}

sync_calendar() {
    load_config

    local url="${ANTO426_GCAL_ICS_URL:-}"
    if [[ -z "$url" ]]; then
        notify "Google Calendar is not configured. Please edit $CONFIG_FILE"
        return 2
    fi

    local tmp past_days future_days timezone count
    past_days="${ANTO426_GCAL_SYNC_PAST_DAYS:-7}"
    future_days="${ANTO426_GCAL_SYNC_FUTURE_DAYS:-120}"
    timezone="${TZ:-Europe/Rome}"
    tmp="$(mktemp)"

    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$url" -o "$tmp"
    elif command -v wget >/dev/null 2>&1; then
        wget -qO "$tmp" "$url"
    else
        notify "Please install curl or wget to synchronize your calendar"
        rm -f "$tmp"
        return 1
    fi

    if [[ ! -x "$REMOTE_SYNC_CORE" ]]; then
        notify "remote_sync_core non compilato. Esegui ~/.config/anto426/install_archpkg.sh"
        rm -f "$tmp"
        return 1
    fi

    if ! "$REMOTE_SYNC_CORE" sync-calendar "$tmp" "$GCAL_EVENTS_FILE" "$past_days" "$future_days" "$timezone"; then
        notify "Google Calendar sync failed"
        rm -f "$tmp"
        return 1
    fi

    mv "$tmp" "$GCAL_ICS_FILE"
    count="$(jq 'length' "$GCAL_EVENTS_FILE" 2>/dev/null || printf '0')"
    notify "Google Calendar updated: $count events"
    notify_calendar_due
}

manual_config() {
    ensure_config
    xdg-open "$CONFIG_FILE" >/dev/null 2>&1 &
}

save_config() {
    local gcal_url="$1"
    local past_days="$2"
    local future_days="$3"
    local interval="$4"
    local neofetch_images="${ANTO426_NEOFETCH_IMAGES:-1}"
    local neofetch_auto_sync="0"
    local neofetch_dir="${ANTO426_NEOFETCH_DIR:-}"

    mkdir -p "$DATA_DIR" "$CALENDAR_DIR"
    cat > "$CONFIG_FILE" <<EOF
# Configuration generated by anto426 sync.
# You can edit this file manually, or reopen the wizard using:
# ~/.config/anto426/remote_sync.sh config

# Google Calendar: secret iCal URL.
export ANTO426_GCAL_ICS_URL=$(shell_quote "$gcal_url")
export ANTO426_GCAL_SYNC_PAST_DAYS=$(shell_quote "$past_days")
export ANTO426_GCAL_SYNC_FUTURE_DAYS=$(shell_quote "$future_days")

# Daemon execution interval in seconds.
export ANTO426_SYNC_INTERVAL=$(shell_quote "$interval")

# Fastfetch/neofetch terminal logo behavior.
# 1 = enable images from ~/Pictures/neofetch.
export ANTO426_NEOFETCH_IMAGES=$(shell_quote "$neofetch_images")
# 0 = never repopulate ~/Pictures/neofetch from sync scripts.
export ANTO426_NEOFETCH_AUTO_SYNC=$(shell_quote "$neofetch_auto_sync")
EOF

    if [[ -n "$neofetch_dir" ]]; then
        printf 'export ANTO426_NEOFETCH_DIR=%s\n' "$(shell_quote "$neofetch_dir")" >> "$CONFIG_FILE"
    fi
}

config_status_message() {
    local calendar_status interval_label

    if [[ -n "${ANTO426_GCAL_ICS_URL:-}" ]]; then
        calendar_status="configured"
    else
        calendar_status="not configured"
    fi

    interval_label="${ANTO426_SYNC_INTERVAL:-900}s"
    printf 'Google Calendar: %s\nDaemon interval: %s' \
        "$calendar_status" \
        "$interval_label"
}

guided_calendar_config() {
    local choice url past_days future_days

    choice="$(
        {
            printf '  ── GOOGLE CALENDAR ──────────────\n'
            printf 'Paste/Update iCal URL\0icon\x1foffice-calendar\n'
            printf 'Disable Google Calendar\0icon\x1fedit-clear\n'
            printf '  ────────────────────────────────\n'
            printf 'Back\0icon\x1fgo-previous\n'
        } |
            rofi_pick_msg "Google Calendar" "Use the secret address in iCal format.\nGoogle Calendar -> Settings -> Integrate Calendar -> Secret address in iCal format."
    )"

    case "$choice" in
        "  ──"*)
            guided_calendar_config
            ;;
        *"Paste/Update"*)
            url="$(rofi_input "iCal URL" "${ANTO426_GCAL_ICS_URL:-}" "Paste the secret link that ends with basic.ics")"
            [[ -z "$url" ]] && return 0
            if [[ ! "$url" =~ ^https?:// || "$url" != *".ics"* ]]; then
                notify "Invalid iCal URL"
                return 1
            fi
            past_days="$(rofi_input "Past days" "${ANTO426_GCAL_SYNC_PAST_DAYS:-7}" "How many days in the past to sync")"
            future_days="$(rofi_input "Future days" "${ANTO426_GCAL_SYNC_FUTURE_DAYS:-120}" "How many days in the future to sync")"
            [[ "$past_days" =~ ^[0-9]+$ ]] || past_days=7
            [[ "$future_days" =~ ^[0-9]+$ ]] || future_days=120
            ANTO426_GCAL_ICS_URL="$url"
            ANTO426_GCAL_SYNC_PAST_DAYS="$past_days"
            ANTO426_GCAL_SYNC_FUTURE_DAYS="$future_days"
            ;;
        *"Disable"*)
            ANTO426_GCAL_ICS_URL=""
            ;;
        *)
            return 0
            ;;
    esac
}

guided_interval_config() {
    local choice custom

    choice="$(
        {
            printf '  ── INTERVAL ─────────────────────\n'
            printf '5 minutes\0icon\x1foffice-calendar\n'
            printf '15 minutes\0icon\x1foffice-calendar\n'
            printf '30 minutes\0icon\x1foffice-calendar\n'
            printf '1 hour\0icon\x1foffice-calendar\n'
            printf 'Custom\0icon\x1fpreferences-system\n'
            printf '  ────────────────────────────────\n'
            printf 'Back\0icon\x1fgo-previous\n'
        } |
            rofi_pick_msg "Sync interval" "How often the daemon updates Calendar"
    )"

    case "$choice" in
        "  ──"*) guided_interval_config ;;
        "5 minutes") ANTO426_SYNC_INTERVAL=300 ;;
        "15 minutes") ANTO426_SYNC_INTERVAL=900 ;;
        "30 minutes") ANTO426_SYNC_INTERVAL=1800 ;;
        "1 hour") ANTO426_SYNC_INTERVAL=3600 ;;
        "Custom")
            custom="$(rofi_input "Sync seconds" "${ANTO426_SYNC_INTERVAL:-900}" "Example: 900 = 15 minutes")"
            [[ "$custom" =~ ^[0-9]+$ && "$custom" -ge 60 ]] && ANTO426_SYNC_INTERVAL="$custom" || notify "Invalid interval"
            ;;
    esac
}

persist_current_config() {
    save_config \
        "${ANTO426_GCAL_ICS_URL:-}" \
        "${ANTO426_GCAL_SYNC_PAST_DAYS:-7}" \
        "${ANTO426_GCAL_SYNC_FUTURE_DAYS:-120}" \
        "${ANTO426_SYNC_INTERVAL:-900}"
}

guided_config() {
    ensure_config
    if ! command -v rofi >/dev/null 2>&1 || [[ -z "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]]; then
        manual_config
        return 0
    fi

    load_config

    while true; do
        local choice
        choice="$(
            {
                printf '  ── CALENDAR ─────────────────────\n'
                printf 'Configure Google Calendar\0icon\x1foffice-calendar\n'
                printf 'Sync Interval\0icon\x1foffice-calendar\n'
                printf '  ── ACTIONS ──────────────────────\n'
                printf 'Save and Test All\0icon\x1fdocument-save\n'
                printf 'Open Advanced Config File\0icon\x1ftext-x-generic\n'
                printf '  ────────────────────────────────\n'
                printf 'Exit\0icon\x1fgo-previous\n'
            } |
                rofi_pick_msg "Sync Config" "$(config_status_message)"
        )"

        case "$choice" in
            "  ──"*) continue ;;
            *"Configure Google Calendar")
                guided_calendar_config
                persist_current_config
                ;;
            *"Sync Interval")
                guided_interval_config
                persist_current_config
                ;;
            *"Save and Test All")
                persist_current_config
                ANTO426_SYNC_QUIET=0 "$0" all
                ;;
            *"Advanced Config File")
                manual_config
                return 0
                ;;
            *)
                persist_current_config
                return 0
                ;;
        esac

        load_config
    done
}

daemon() {
    if ! mkdir "$DAEMON_LOCK_DIR" 2>/dev/null; then
        local old_pid
        old_pid="$(cat "$DAEMON_LOCK_DIR/pid" 2>/dev/null || true)"
        if [[ -n "$old_pid" ]] && kill -0 "$old_pid" 2>/dev/null; then
            exit 0
        fi

        rm -f "$DAEMON_LOCK_DIR/pid" 2>/dev/null || true
        rmdir "$DAEMON_LOCK_DIR" 2>/dev/null || true
        mkdir "$DAEMON_LOCK_DIR" 2>/dev/null || exit 0
    fi
    printf '%s\n' "$$" > "$DAEMON_LOCK_DIR/pid"
    trap 'rm -f "$DAEMON_LOCK_DIR/pid" 2>/dev/null || true; rmdir "$DAEMON_LOCK_DIR" 2>/dev/null || true' EXIT INT TERM

    load_config
    local interval="${ANTO426_SYNC_INTERVAL:-900}"

    while true; do
        ANTO426_SYNC_QUIET=1 "$0" calendar >/dev/null 2>&1 || true
        sleep "$interval"
    done
}

case "${1:-all}" in
    init)
        ensure_config
        notify "Config created: $CONFIG_FILE"
        ;;
    config)
        guided_config
        ;;
    calendar)
        sync_calendar
        ;;
    all)
        sync_calendar || true
        ;;
    daemon)
        daemon
        ;;
    *)
        printf 'Usage: %s [init|config|calendar|all|daemon]\n' "$0" >&2
        exit 2
        ;;
esac
