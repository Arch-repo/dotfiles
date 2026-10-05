#!/usr/bin/env bash
set -euo pipefail

backend="${1:?percorso backend mancante}"
project="$(cd "$(dirname "$0")/.." && pwd)"
temporary="$(mktemp -d)"
trap 'rm -rf -- "$temporary"' EXIT

fail() {
    printf 'calendar/display fixture: %s\n' "$*" >&2
    exit 1
}

require_text() {
    local output="$1" wanted="$2"
    [[ "$output" == *"$wanted"* ]] || {
        printf 'Output mancante: %s\n---\n%s\n' "$wanted" "$output" >&2
        exit 1
    }
}

export ANTO_MENU_NO_NOTIFY=1
export ANTO_CALENDAR_EVENTS_FILE="$temporary/calendar/events.json"
export ANTO_CALENDAR_LOCK_FILE="$temporary/calendar.lock"

first_id="$(printf '%s' '{
  "title":"  Evento\u000alocale  ",
  "date":"2026-08-02",
  "start":"10:00",
  "end":"11:00",
  "description":"Fixture",
  "all_day":false
}' | "$backend" calendar add)"
[[ "$first_id" == local-* ]] || fail "ID calendario non nativo"
second_id="$(printf '%s' '{
  "title":"Tutto il giorno",
  "date":"2026-08-01",
  "start":"99:99",
  "end":"00:00",
  "description":"",
  "all_day":true
}' | "$backend" calendar add)"
calendar="$($backend calendar list)"
require_text "$calendar" '"title": "Tutto il giorno"'
[[ "$(jq -r '.[1].title' <<<"$calendar")" == 'Evento locale' ]] ||
    fail "normalizzazione titolo calendario inattesa"
[[ "$(jq -r '.[0].id' <<<"$calendar")" == "$second_id" ]] ||
    fail "ordinamento calendario non deterministico"
[[ "$(stat -c '%a' "$ANTO_CALENDAR_EVENTS_FILE")" == 600 ]] ||
    fail "permessi calendario non restrittivi"

before="$(sha256sum "$ANTO_CALENDAR_EVENTS_FILE")"
if printf '%s' '{"title":"No","date":"2026-02-30","start":"09:00"}' |
    "$backend" calendar add >/dev/null 2>&1; then
    fail "data calendario impossibile accettata"
fi
[[ "$(sha256sum "$ANTO_CALENDAR_EVENTS_FILE")" == "$before" ]] ||
    fail "evento invalido ha modificato il calendario"
"$backend" calendar delete "$first_id"
[[ "$(jq --arg id "$first_id" '[.[] | select(.id == $id)] | length' \
    "$ANTO_CALENDAR_EVENTS_FILE")" == 0 ]] || fail "delete calendario fallita"

export ANTO_MENU_BACKEND="$backend"
wrapper_calendar="$(printf '%s' '{
  "title":"Wrapper",
  "date":"2026-08-03",
  "start":"",
  "end":"",
  "description":"",
  "all_day":true
}' | "$project/scripts/actions/calendar.sh" add)"
[[ "$wrapper_calendar" == local-* ]] || fail "shim calendario non trasparente"

cp "$ANTO_CALENDAR_EVENTS_FILE" "$temporary/calendar-good.json"
printf '{broken\n' >"$ANTO_CALENDAR_EVENTS_FILE"
if "$backend" calendar list >/dev/null 2>&1; then
    fail "archivio calendario corrotto accettato"
fi
cmp <(printf '{broken\n') "$ANTO_CALENDAR_EVENTS_FILE" ||
    fail "archivio calendario corrotto modificato"
cp "$temporary/calendar-good.json" "$ANTO_CALENDAR_EVENTS_FILE"

monitor_file="$temporary/monitors.json"
virtual_state="$temporary/virtual.json"
cat >"$monitor_file" <<'JSON'
[
  {
    "name":"eDP-1", "description":"Pannello interno", "disabled":false,
    "width":2560, "height":1600, "refreshRate":120.0,
    "x":1920, "y":0, "scale":1.6, "transform":0,
    "mirrorOf":"none", "focused":true, "dpmsStatus":true,
    "activeWorkspace":{"name":"eDP·1"},
    "availableModes":["2560x1600@120.00Hz","1920x1200@60.00Hz"]
  },
  {
    "name":"HDMI-A-1", "description":"LG ULTRAGEAR", "disabled":false,
    "width":1920, "height":1080, "refreshRate":143.98,
    "x":0, "y":0, "scale":1.0, "transform":0,
    "mirrorOf":"none", "focused":false, "dpmsStatus":true,
    "activeWorkspace":{"name":"HDMI·1"},
    "availableModes":["1920x1080@143.98Hz","1920x1080@60.00Hz"]
  },
  {
    "name":"ANTO-VIRTUAL-HDMI-2", "description":"Virtuale", "disabled":true,
    "width":1920, "height":1080, "refreshRate":60.0,
    "x":3520, "y":0, "scale":1.0, "transform":0,
    "mirrorOf":"none", "focused":false, "dpmsStatus":false,
    "availableModes":["1920x1080@60.00Hz"]
  }
]
JSON
cat >"$virtual_state" <<'JSON'
{"version":1,"outputs":[{"name":"ANTO-VIRTUAL-HDMI-2"}]}
JSON

export ANTO_DISPLAY_MONITORS_JSON_FILE="$monitor_file"
export ANTO_VIRTUAL_STATE_FILE="$virtual_state"
export ANTO_LOCAL_CONFIG_ROOT="$temporary/local"
export ANTO_DISPLAY_PROFILE_DIR="$temporary/local/display/profiles"
export ANTO_DISPLAY_PERSIST_FILE="$temporary/local/hypr/monitors.conf"
export ANTO_DISPLAY_LOCK_FILE="$temporary/display.lock"

status="$($backend display status)"
require_text "$status" '2 attivi su 3 collegati'
list="$($backend display list)"
require_text "$list" $'eDP-1\tPannello interno\ton\t2560x1600'
require_text "$list" $'HDMI-A-1\tLG ULTRAGEAR\ton\t1920x1080'
modes="$($backend display modes HDMI-A-1)"
require_text "$modes" '1920x1080@143.98'

preview="$($backend display persistent-preview)"
require_text "$preview" 'monitor=eDP-1,2560x1600@120,1920x0,1.6,transform,0'
require_text "$preview" 'monitor=HDMI-A-1,1920x1080@143.98,0x0,1,transform,0'
[[ "$preview" != *'ANTO-VIRTUAL-'* ]] ||
    fail "monitor virtuale scritto nella config fisica"

cp "$virtual_state" "$temporary/virtual-good.json"
printf '{"version":1,"outputs":[{"name":"HDMI-A-1"}]}\n' >"$virtual_state"
if "$backend" display persistent-preview >/dev/null 2>&1; then
    fail "stato virtuale non namespaced accettato"
fi
cp "$temporary/virtual-good.json" "$virtual_state"

"$backend" display persist-current
cmp <(printf '%s' "$preview") <(sed -z 's/\n$//' "$ANTO_DISPLAY_PERSIST_FILE") ||
    fail "persistenza C diversa dall'anteprima"
[[ "$(stat -c '%a' "$ANTO_DISPLAY_PERSIST_FILE")" == 600 ]] ||
    fail "permessi monitors.conf non restrittivi"
"$backend" display profile-save dock
profiles="$($backend display profile-list)"
require_text "$profiles" $'dock\teDP-1 + HDMI-A-1'
"$backend" display profile-delete dock
[[ ! -e "$ANTO_DISPLAY_PROFILE_DIR/dock.json" ]] ||
    fail "profilo display non eliminato"

dry="$(ANTO_DISPLAY_DRY_RUN=1 "$backend" display scale eDP-1 2)"
require_text "$dry" 'DRY-RUN hyprctl keyword monitor eDP-1,2560x1600@120,1920x0,2,transform,0'
if ANTO_DISPLAY_DRY_RUN=1 "$backend" display disable NOPE >/dev/null 2>&1; then
    fail "monitor inesistente accettato"
fi
rollback="$(ANTO_DISPLAY_MOCK_APPLY=1 ANTO_DISPLAY_CONFIRM_RESPONSE=revert \
    "$backend" display disable HDMI-A-1)"
require_text "$rollback" 'MOCK hyprctl keyword monitor HDMI-A-1,disable'
require_text "$rollback" 'MOCK hyprctl keyword monitor eDP-1,2560x1600@120,1920x0,1.6,transform,0'
layout='[{"name":"HDMI-A-1","x":0,"y":0},{"name":"eDP-1","x":1920,"y":0}]'
dry="$(ANTO_DISPLAY_DRY_RUN=1 "$backend" display arrange "$layout")"
require_text "$dry" 'DRY-RUN virtual-output update-layout'
require_text "$dry" 'DRY-RUN persist'
overlap='[{"name":"HDMI-A-1","x":0,"y":0},{"name":"eDP-1","x":100,"y":100}]'
if ANTO_DISPLAY_DRY_RUN=1 "$backend" display arrange "$overlap" >/dev/null 2>&1; then
    fail "layout sovrapposto accettato"
fi

wrapper_list="$($project/scripts/actions/display.sh list)"
require_text "$wrapper_list" $'eDP-1\tPannello interno'

printf 'calendar/display fixtures: ok\n'
