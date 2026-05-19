#!/usr/bin/env bash
set -uo pipefail

INPUT_CONF="$HOME/.config/hypr/conf/input.conf"
RULES_FILE="/usr/share/X11/xkb/rules/base.lst"

system_locale() {
    local locale_name
    locale_name="${ANTO426_UI_LOCALE:-}"
    if [[ -z "$locale_name" ]]; then
        locale_name="$(
            localectl status 2>/dev/null |
                awk -F'LANG=' '/System Locale:/ {print $2; exit}'
        )"
    fi
    printf '%s' "${locale_name:-${LANG:-C.UTF-8}}"
}

UI_LOCALE="$(system_locale)"

system_text() {
    local msgid="$1"
    local domain translated

    if ! command -v gettext >/dev/null 2>&1; then
        printf '%s' "$msgid"
        return 0
    fi

    for domain in gtk40 gtk30 NetworkManager blueman xkeyboard-config; do
        translated="$(LC_ALL="$UI_LOCALE" gettext -d "$domain" "$msgid" 2>/dev/null || true)"
        if [[ -n "$translated" && "$translated" != "$msgid" ]]; then
            printf '%s' "$translated"
            return 0
        fi
    done

    printf '%s' "$msgid"
}

layout_codes() {
    local configured
    configured="$(
        awk -F= '
            /^[[:space:]]*kb_layout[[:space:]]*=/ {
                gsub(/[[:space:]]/, "", $2)
                print $2
                exit
            }
        ' "$INPUT_CONF" 2>/dev/null
    )"

    if [[ -z "$configured" ]]; then
        configured="$(localectl status 2>/dev/null | awk -F: '/X11 Layout:/ {gsub(/^[[:space:]]+/, "", $2); print $2; exit}')"
    fi

    printf '%s\n' "${configured:-it}" |
        tr ',' '\n' |
        awk 'NF && !seen[$0]++ {print}'
}

layout_raw_description() {
    local code="$1"
    awk -v code="$code" '
        /^! layout/ {in_layout = 1; next}
        /^!/ {in_layout = 0}
        in_layout && $1 == code {
            $1 = ""
            sub(/^[[:space:]]+/, "")
            print
            exit
        }
    ' "$RULES_FILE" 2>/dev/null
}

layout_description() {
    local code="$1"
    local raw
    raw="$(layout_raw_description "$code")"
    raw="${raw:-$code}"

    if command -v gettext >/dev/null 2>&1; then
        LC_ALL="$UI_LOCALE" gettext -d xkeyboard-config "$raw" 2>/dev/null || printf '%s' "$raw"
    else
        printf '%s' "$raw"
    fi
}

active_keymap() {
    if command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
        hyprctl devices -j 2>/dev/null |
            jq -r '.keyboards[]? | select(.main == true) | .active_keymap // empty' |
            sed -n '1p'
        return 0
    fi

    printf ''
}

active_code() {
    local active="$1"
    local code raw translated

    while IFS= read -r code; do
        raw="$(layout_raw_description "$code")"
        translated="$(layout_description "$code")"
        if [[ "$active" == "$raw" || "$active" == "$translated" ]]; then
            printf '%s' "$code"
            return 0
        fi
    done < <(layout_codes)

    case "$active" in
        *Italian*) printf 'it' ;;
        *"English (US)"* | *English*) printf 'us' ;;
        *) layout_codes | sed -n '1p' ;;
    esac
}

json_output() {
    python3 - "$1" "$2" "$3" <<'PY'
import json
import sys

print(json.dumps({
    "text": sys.argv[1],
    "tooltip": sys.argv[2],
    "class": sys.argv[3],
}, ensure_ascii=False))
PY
}

active="$(active_keymap)"
code="$(active_code "$active")"
code="${code:-it}"
description="$(layout_description "$code")"
label="$(printf '%s' "$code" | tr '[:lower:]' '[:upper:]')"

json_output " $label" "$(system_text "Keyboard"): $description" "$code"
