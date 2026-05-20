#!/usr/bin/env bash
set -uo pipefail

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anto426"
CONFIG_FILE="$DATA_DIR/sync.env"
CALENDAR_DIR="$DATA_DIR/calendar"
GCAL_ICS_FILE="$CALENDAR_DIR/google.ics"
GCAL_EVENTS_FILE="$CALENDAR_DIR/google_events.json"
GCAL_REMINDER_STATE_FILE="$CALENDAR_DIR/google_reminders.sent"
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
    rofi -dmenu -i -matching fuzzy -p "$prompt" -mesg "$message" -theme "$THEME_SETUP"
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

ensure_config() {
    mkdir -p "$DATA_DIR" "$CALENDAR_DIR"

    if [[ ! -f "$CONFIG_FILE" ]]; then
        cat > "$CONFIG_FILE" <<'EOF'
# Config locale, non dipende da Codex.
#
# Google Calendar:
# 1. Apri Google Calendar nel browser.
# 2. Impostazioni calendario -> Integra calendario.
# 3. Copia "Indirizzo segreto in formato iCal" e incollalo qui.
# export ANTO426_GCAL_ICS_URL='https://calendar.google.com/calendar/ical/.../basic.ics'
#
# Opzionali:
# export ANTO426_GCAL_SYNC_PAST_DAYS=7
# export ANTO426_GCAL_SYNC_FUTURE_DAYS=120
# export ANTO426_SYNC_INTERVAL=900
EOF
    fi
}

load_config() {
    ensure_config
    # shellcheck disable=SC1090
    source "$CONFIG_FILE"
}

notify_calendar_due() {
    [[ -s "$GCAL_EVENTS_FILE" ]] || return 0
    command -v python3 >/dev/null 2>&1 || return 0
    command -v notify-send >/dev/null 2>&1 || return 0

    local lookback timezone
    lookback="${ANTO426_GCAL_REMINDER_LOOKBACK:-${ANTO426_SYNC_INTERVAL:-900}}"
    [[ "$lookback" =~ ^[0-9]+$ ]] || lookback=900
    timezone="${TZ:-Europe/Rome}"

    python3 - "$GCAL_EVENTS_FILE" "$GCAL_REMINDER_STATE_FILE" "$lookback" "$timezone" <<'PY' |
import json
import sys
from datetime import datetime, time, timedelta
from pathlib import Path
from zoneinfo import ZoneInfo

events_path, state_path, lookback_seconds, tz_name = sys.argv[1:5]
try:
    lookback_seconds = int(lookback_seconds)
except ValueError:
    lookback_seconds = 900
lookback_seconds = min(max(lookback_seconds, 60), 7200)

try:
    local_tz = ZoneInfo(tz_name)
except Exception:
    local_tz = ZoneInfo("Europe/Rome")

def clean(value, limit=180):
    text = str(value or "").replace("\t", " ").replace("\r", " ").replace("\n", " ")
    text = " ".join(text.split())
    if len(text) > limit:
        return text[: limit - 3].rstrip() + "..."
    return text

try:
    with open(events_path, "r", encoding="utf-8") as fh:
        events = json.load(fh)
except Exception:
    sys.exit(0)

state = Path(state_path)
try:
    sent = {line.strip() for line in state.read_text(encoding="utf-8").splitlines() if line.strip()}
except Exception:
    sent = set()

now = datetime.now(local_tz)
recent_keys = set()
new_keys = set()
notifications = []

for event in events if isinstance(events, list) else []:
    if not isinstance(event, dict):
        continue

    date_text = clean(event.get("date"), 20)
    start_text = clean(event.get("start"), 12)
    title = clean(event.get("title") or "Evento", 120)
    is_all_day = bool(event.get("all_day"))

    try:
        event_date = datetime.strptime(date_text, "%Y-%m-%d").date()
        if is_all_day:
            start_dt = datetime.combine(event_date, time(hour=9), local_tz)
            start_label = "Tutto il giorno"
        else:
            if not start_text:
                continue
            start_dt = datetime.strptime(f"{date_text} {start_text}", "%Y-%m-%d %H:%M").replace(tzinfo=local_tz)
            end_text = clean(event.get("end"), 12)
            start_label = f"{start_text}-{end_text}" if end_text else start_text
    except Exception:
        continue

    key = f"{event.get('id') or title}|{date_text}|{start_text or 'all-day'}"
    if start_dt >= now - timedelta(days=3):
        recent_keys.add(key)

    elapsed = (now - start_dt).total_seconds()
    if key in sent or elapsed < 0 or elapsed > lookback_seconds:
        continue

    description = clean(event.get("description"), 160)
    body = f"{start_label} - {title}"
    if description:
        body = f"{body} - {description}"
    notifications.append(("Calendario", body[:500], key))
    new_keys.add(key)

try:
    state.parent.mkdir(parents=True, exist_ok=True)
    kept = (sent & recent_keys) | new_keys
    state.write_text("\n".join(sorted(kept)) + ("\n" if kept else ""), encoding="utf-8")
except Exception:
    pass

for title, body, _key in notifications:
    print(f"{title}\t{body}")
PY
    while IFS=$'\t' read -r title body; do
        [[ -n "$title" ]] || continue
        notify-send -a "anto426 Calendar" -i "x-office-calendar" "$title" "$body" 2>/dev/null || true
    done
}

sync_calendar() {
    load_config

    local url="${ANTO426_GCAL_ICS_URL:-}"
    if [[ -z "$url" ]]; then
        notify "Google Calendar non configurato. Modifica $CONFIG_FILE"
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
        notify "Installa curl o wget per sincronizzare il calendario"
        rm -f "$tmp"
        return 1
    fi

    python3 - "$tmp" "$GCAL_EVENTS_FILE" "$past_days" "$future_days" "$timezone" <<'PY'
import json
import re
import sys
from datetime import date, datetime, time, timedelta, timezone
from zoneinfo import ZoneInfo

ics_path, out_path, past_days, future_days, local_tz_name = sys.argv[1:6]
past_days = int(past_days)
future_days = int(future_days)
local_tz = ZoneInfo(local_tz_name)
window_start = datetime.combine(date.today() - timedelta(days=past_days), time.min, local_tz)
window_end = datetime.combine(date.today() + timedelta(days=future_days), time.max, local_tz)

def unfold(lines):
    out = []
    for line in lines:
        line = line.rstrip("\r\n")
        if line.startswith((" ", "\t")) and out:
            out[-1] += line[1:]
        else:
            out.append(line)
    return out

def split_prop(line):
    if ":" not in line:
        return None, {}, ""
    left, value = line.split(":", 1)
    parts = left.split(";")
    name = parts[0].upper()
    params = {}
    for part in parts[1:]:
        if "=" in part:
            key, val = part.split("=", 1)
            params[key.upper()] = val.strip('"')
    return name, params, value

def unescape_text(value):
    return (
        value.replace("\\n", " ")
        .replace("\\N", " ")
        .replace("\\,", ",")
        .replace("\\;", ";")
        .replace("\\\\", "\\")
        .strip()
    )

def parse_dt(value, params):
    if params.get("VALUE") == "DATE" or re.fullmatch(r"\d{8}", value):
        return datetime.strptime(value[:8], "%Y%m%d").date(), True

    raw = value
    tz = local_tz
    if raw.endswith("Z"):
        tz = timezone.utc
        raw = raw[:-1]
    elif "TZID" in params:
        try:
            tz = ZoneInfo(params["TZID"])
        except Exception:
            tz = local_tz

    fmt = "%Y%m%dT%H%M%S" if len(raw) >= 15 else "%Y%m%dT%H%M"
    dt = datetime.strptime(raw[:15] if len(raw) >= 15 else raw, fmt).replace(tzinfo=tz)
    return dt.astimezone(local_tz), False

def parse_until(value):
    if not value:
        return None
    try:
        parsed, all_day = parse_dt(value, {})
        if all_day:
            return datetime.combine(parsed, time.max, local_tz)
        return parsed
    except Exception:
        return None

def parse_rrule(value):
    rule = {}
    for part in value.split(";"):
        if "=" in part:
            key, val = part.split("=", 1)
            rule[key.upper()] = val
    return rule

def add_months(dt, months):
    month = dt.month - 1 + months
    year = dt.year + month // 12
    month = month % 12 + 1
    day = min(dt.day, [31, 29 if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31][month - 1])
    return dt.replace(year=year, month=month, day=day)

def expand_starts(start, rule):
    if not rule:
        return [start]

    freq = rule.get("FREQ", "").upper()
    interval = max(int(rule.get("INTERVAL", "1")), 1)
    count_limit = int(rule.get("COUNT", "1000"))
    until = parse_until(rule.get("UNTIL", ""))
    starts = []
    seen = 0
    current = start
    max_seen = min(count_limit, 1500)

    def allowed(dt):
        if until and dt > until:
            return False
        return True

    if freq == "DAILY":
        while seen < max_seen and current <= window_end:
            if allowed(current):
                starts.append(current)
            current += timedelta(days=interval)
            seen += 1
    elif freq == "WEEKLY":
        day_map = {"MO": 0, "TU": 1, "WE": 2, "TH": 3, "FR": 4, "SA": 5, "SU": 6}
        bydays = [day_map[d] for d in rule.get("BYDAY", "").split(",") if d in day_map]
        if not bydays:
            bydays = [start.weekday()]
        week_start = start - timedelta(days=start.weekday())
        while seen < max_seen and week_start <= window_end:
            for day_num in bydays:
                candidate = (week_start + timedelta(days=day_num)).replace(
                    hour=start.hour, minute=start.minute, second=start.second, microsecond=start.microsecond
                )
                if candidate >= start and allowed(candidate):
                    starts.append(candidate)
            week_start += timedelta(weeks=interval)
            seen += len(bydays)
    elif freq == "MONTHLY":
        bymonthdays = [int(x) for x in rule.get("BYMONTHDAY", "").split(",") if x.lstrip("-").isdigit()]
        while seen < max_seen and current <= window_end:
            candidates = [current]
            if bymonthdays:
                candidates = []
                for monthday in bymonthdays:
                    try:
                        candidates.append(current.replace(day=monthday))
                    except ValueError:
                        pass
            for candidate in candidates:
                if candidate >= start and allowed(candidate):
                    starts.append(candidate)
            current = add_months(current, interval)
            seen += max(len(candidates), 1)
    elif freq == "YEARLY":
        current_year = start.year
        while seen < max_seen and current_year <= window_end.year:
            try:
                candidate = start.replace(year=current_year)
            except ValueError:
                current_year += interval
                seen += 1
                continue
            if candidate >= start and allowed(candidate):
                starts.append(candidate)
            current_year += interval
            seen += 1
    else:
        starts.append(start)

    return starts

with open(ics_path, "r", encoding="utf-8", errors="replace") as fh:
    lines = unfold(fh.readlines())

events = []
current = None
for line in lines:
    name, params, value = split_prop(line)
    if name == "BEGIN" and value == "VEVENT":
        current = {}
    elif name == "END" and value == "VEVENT" and current is not None:
        events.append(current)
        current = None
    elif current is not None and name:
        if name in ("DTSTART", "DTEND"):
            current[name] = (value, params)
        elif name in ("UID", "SUMMARY", "DESCRIPTION", "LOCATION", "URL", "RRULE"):
            current[name] = value

out = []
for event in events:
    if "DTSTART" not in event:
        continue

    try:
        start, all_day = parse_dt(*event["DTSTART"])
    except Exception:
        continue

    end = None
    if "DTEND" in event:
        try:
            end, _ = parse_dt(*event["DTEND"])
        except Exception:
            end = None

    if all_day:
        start_dt = datetime.combine(start, time.min, local_tz)
        end_dt = datetime.combine(end if isinstance(end, date) else start + timedelta(days=1), time.min, local_tz)
    else:
        start_dt = start
        end_dt = end if isinstance(end, datetime) else start_dt + timedelta(hours=1)

    duration = max(end_dt - start_dt, timedelta(minutes=1))
    rule = parse_rrule(event.get("RRULE", ""))
    title = unescape_text(event.get("SUMMARY", "Evento"))
    description = unescape_text(event.get("DESCRIPTION", ""))
    location = unescape_text(event.get("LOCATION", ""))
    if location and location not in description:
        description = (description + "  " if description else "") + f"Luogo: {location}"
    uid = unescape_text(event.get("UID", title))
    url = unescape_text(event.get("URL", ""))

    for occ_start in expand_starts(start_dt, rule):
        occ_end = occ_start + duration
        if occ_end < window_start or occ_start > window_end:
            continue

        if all_day:
            current_day = occ_start.date()
            last_day = max(current_day, (occ_end - timedelta(seconds=1)).date())
            while current_day <= last_day:
                if window_start.date() <= current_day <= window_end.date():
                    out.append({
                        "id": f"gcal-{uid}-{current_day.isoformat()}",
                        "source": "google",
                        "date": current_day.isoformat(),
                        "start": "",
                        "end": "",
                        "title": title,
                        "description": description,
                        "url": url,
                        "all_day": True,
                    })
                current_day += timedelta(days=1)
        else:
            out.append({
                "id": f"gcal-{uid}-{occ_start.isoformat()}",
                "source": "google",
                "date": occ_start.date().isoformat(),
                "start": occ_start.strftime("%H:%M"),
                "end": occ_end.strftime("%H:%M"),
                "title": title,
                "description": description,
                "url": url,
                "all_day": False,
            })

out.sort(key=lambda item: (item["date"], item["start"] or "00:00", item["title"]))
with open(out_path, "w", encoding="utf-8") as fh:
    json.dump(out, fh, ensure_ascii=False, indent=2)
    fh.write("\n")
PY

    mv "$tmp" "$GCAL_ICS_FILE"
    count="$(jq 'length' "$GCAL_EVENTS_FILE" 2>/dev/null || printf '0')"
    notify "Google Calendar aggiornato: $count eventi"
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

    mkdir -p "$DATA_DIR" "$CALENDAR_DIR"
    cat > "$CONFIG_FILE" <<EOF
# Config generata da anto426 sync.
# Puoi modificarla a mano, oppure riaprire il wizard con:
# ~/.config/anto426/remote_sync.sh config

# Google Calendar: URL segreto iCal.
export ANTO426_GCAL_ICS_URL=$(shell_quote "$gcal_url")
export ANTO426_GCAL_SYNC_PAST_DAYS=$(shell_quote "$past_days")
export ANTO426_GCAL_SYNC_FUTURE_DAYS=$(shell_quote "$future_days")

# Intervallo daemon in secondi.
export ANTO426_SYNC_INTERVAL=$(shell_quote "$interval")
EOF
}

config_status_message() {
    local calendar_status interval_label

    if [[ -n "${ANTO426_GCAL_ICS_URL:-}" ]]; then
        calendar_status="configurato"
    else
        calendar_status="non configurato"
    fi

    interval_label="${ANTO426_SYNC_INTERVAL:-900}s"
    printf 'Google Calendar: %s\nIntervallo daemon: %s' \
        "$calendar_status" \
        "$interval_label"
}

guided_calendar_config() {
    local choice url past_days future_days

    choice="$(
        printf '%s\n' \
            "󰁔 Incolla/aggiorna URL iCal" \
            "󰅖 Disattiva Google Calendar" \
            "󰌍 Indietro" |
            rofi_pick_msg "Google Calendar" "Usa l'indirizzo segreto in formato iCal.\nGoogle Calendar -> Impostazioni calendario\nIntegra calendario -> Indirizzo segreto iCal."
    )"

    case "$choice" in
        *"Incolla"*)
            url="$(rofi_input "URL iCal" "${ANTO426_GCAL_ICS_URL:-}" "Incolla il link segreto che finisce con basic.ics")"
            [[ -z "$url" ]] && return 0
            past_days="$(rofi_input "Giorni passati" "${ANTO426_GCAL_SYNC_PAST_DAYS:-7}" "Quanti giorni nel passato sincronizzare")"
            future_days="$(rofi_input "Giorni futuri" "${ANTO426_GCAL_SYNC_FUTURE_DAYS:-120}" "Quanti giorni nel futuro sincronizzare")"
            [[ "$past_days" =~ ^[0-9]+$ ]] || past_days=7
            [[ "$future_days" =~ ^[0-9]+$ ]] || future_days=120
            ANTO426_GCAL_ICS_URL="$url"
            ANTO426_GCAL_SYNC_PAST_DAYS="$past_days"
            ANTO426_GCAL_SYNC_FUTURE_DAYS="$future_days"
            ;;
        *"Disattiva"*)
            ANTO426_GCAL_ICS_URL=""
            ;;
        *)
            return 0
            ;;
    esac
}

guided_interval_config() {
    local choice

    choice="$(
        printf '%s\n' \
            "5 minuti" \
            "15 minuti" \
            "30 minuti" \
            "1 ora" |
            rofi_pick_msg "Intervallo sync" "Ogni quanto il daemon aggiorna Calendar"
    )"

    case "$choice" in
        "5 minuti") ANTO426_SYNC_INTERVAL=300 ;;
        "15 minuti") ANTO426_SYNC_INTERVAL=900 ;;
        "30 minuti") ANTO426_SYNC_INTERVAL=1800 ;;
        "1 ora") ANTO426_SYNC_INTERVAL=3600 ;;
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
            printf '%s\n' \
                "󰃭 Configura Google Calendar" \
                "󰔚 Intervallo sincronizzazione" \
                "󰑓 Salva e testa tutto" \
                "󰈙 Apri file avanzato" \
                "󰌍 Esci" |
                rofi_pick_msg "Config sync" "$(config_status_message)"
        )"

        case "$choice" in
            *"Google Calendar")
                guided_calendar_config
                persist_current_config
                ;;
            *"Intervallo")
                guided_interval_config
                persist_current_config
                ;;
            *"Salva e testa")
                persist_current_config
                ANTO426_SYNC_QUIET=0 "$0" all
                ;;
            *"file avanzato")
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
        notify "Config creata: $CONFIG_FILE"
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
        printf 'Uso: %s [init|config|calendar|all|daemon]\n' "$0" >&2
        exit 2
        ;;
esac
