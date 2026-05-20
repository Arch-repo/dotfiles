#!/usr/bin/env bash
set -uo pipefail

THEME="${ROFI_BACKGROUND_APPS_THEME:-$HOME/.config/rofi/control_menu.rasi}"

json_escape() {
    local value="${1:-}"
    value=${value//\\/\\\\}
    value=${value//\"/\\\"}
    value=${value//$'\n'/\\n}
    value=${value//$'\r'/}
    printf '%s' "$value"
}

require_stack() {
    command -v hyprctl >/dev/null 2>&1 &&
        command -v jq >/dev/null 2>&1
}

clients_json() {
    hyprctl clients -j 2>/dev/null || printf '[]'
}

active_address() {
    hyprctl activewindow -j 2>/dev/null |
        jq -r '.address // empty' 2>/dev/null ||
        printf ''
}

background_clients_filter='
    map(select(
        (.mapped // true) == true
        and (.hidden // false) == false
        and ((.address // "") != $active_address)
        and ((.class // .initialClass // "") | test("^(rofi|waybar|swaync|swaync-control-center|wofi)$"; "i") | not)
        and ((.title // "") | length > 0)
    ))
'

app_count() {
    local active
    active="$(active_address)"
    clients_json | jq --arg active_address "$active" "$background_clients_filter | length" 2>/dev/null || printf '0'
}

app_tooltip() {
    local active
    active="$(active_address)"
    clients_json |
        jq -r --arg active_address "$active" "$background_clients_filter | sort_by(.workspace.name, .class, .title) |
            if length == 0 then
                \"Nessuna app in background\"
            else
                [\"App in background\"] +
                ([.[0:12][] |
                    \"• \" + ((.class // .initialClass // \"App\") | tostring) +
                    \" — \" + ((.title // \"\") | tostring) +
                    \" [\" + ((.workspace.name // .workspace.id // \"?\") | tostring) + \"]\"
                ]) +
                (if length > 12 then [\"• …\"] else [] end) |
                join(\"\\n\")
            end" 2>/dev/null ||
        printf 'App in background'
}

waybar_status() {
    local count tooltip class text

    if ! require_stack; then
        printf '{"text":"󰘔","tooltip":"Hyprland/jq non disponibile","class":"missing"}\n'
        return 0
    fi

    count="$(app_count)"
    [[ "$count" =~ ^[0-9]+$ ]] || count=0
    tooltip="$(app_tooltip)"
    class="empty"
    text="󰘔"

    if ((count > 0)); then
        class="active"
        text="󰘔 ${count}"
    fi

    printf '{"text":"%s","tooltip":"%s","class":"%s"}\n' \
        "$(json_escape "$text")" \
        "$(json_escape "$tooltip")" \
        "$class"
}

desktop_icon_for_class() {
    local class="${1:-}"
    local initial_class="${2:-}"
    local class_lc initial_lc file base_lc startup_lc icon

    class_lc="$(printf '%s' "$class" | tr '[:upper:]' '[:lower:]')"
    initial_lc="$(printf '%s' "$initial_class" | tr '[:upper:]' '[:lower:]')"

    while IFS= read -r -d '' file; do
        base_lc="$(basename "$file" .desktop | tr '[:upper:]' '[:lower:]')"
        startup_lc="$(
            awk -F= 'tolower($1) == "startupwmclass" {print tolower($2); exit}' "$file" 2>/dev/null
        )"

        if [[ "$base_lc" == "$class_lc" ||
              "$base_lc" == "$initial_lc" ||
              "$startup_lc" == "$class_lc" ||
              "$startup_lc" == "$initial_lc" ]]; then
            icon="$(awk -F= 'tolower($1) == "icon" {print $2; exit}' "$file" 2>/dev/null)"
            [[ -n "$icon" ]] && {
                printf '%s' "$icon"
                return 0
            }
        fi
    done < <(
        find \
            "$HOME/.local/share/applications" \
            "$HOME/.local/share/flatpak/exports/share/applications" \
            /var/lib/flatpak/exports/share/applications \
            /usr/local/share/applications \
            /usr/share/applications \
            -maxdepth 1 -type f -name '*.desktop' -print0 2>/dev/null
    )

    printf '%s' "${class_lc:-application-x-executable}"
}

menu_rows() {
    local line address class initial_class title workspace icon label
    local active

    active="$(active_address)"

    clients_json |
        jq -r --arg active_address "$active" "$background_clients_filter | sort_by(.workspace.name, .class, .title) |
            .[] | [
                (.address // \"\"),
                (.class // \"App\"),
                (.initialClass // .class // \"App\"),
                ((.title // \"\") | gsub(\"[\\t\\n\\r]\"; \" \")),
                ((.workspace.name // .workspace.id // \"?\") | tostring)
            ] | @tsv" |
        while IFS=$'\t' read -r address class initial_class title workspace; do
            [[ -n "$address" ]] || continue
            icon="$(desktop_icon_for_class "$class" "$initial_class")"
            label="${class}"
            [[ -n "$title" ]] && label="${label}  ${title}"
            label="${label}  [${workspace}]"
            printf '%s\0icon\x1f%s\n' "$label" "$icon"
        done
}

address_for_index() {
    local index="$1"
    local active
    active="$(active_address)"

    clients_json |
        jq -r --arg active_address "$active" --argjson index "$index" "$background_clients_filter |
            sort_by(.workspace.name, .class, .title) |
            .[\$index].address // empty" 2>/dev/null
}

open_menu() {
    local count choice address

    require_stack || {
        notify-send "App background" "Hyprland o jq non disponibile" 2>/dev/null || true
        return 1
    }

    count="$(app_count)"
    [[ "$count" =~ ^[0-9]+$ ]] || count=0
    if ((count == 0)); then
        notify-send "App background" "Nessuna app in background" 2>/dev/null || true
        return 0
    fi

    pkill -x rofi 2>/dev/null || true
    choice="$(
        menu_rows |
            rofi -dmenu -i -matching fuzzy -show-icons -format i \
                -p "App background" \
                -theme "$THEME"
    )"

    [[ "$choice" =~ ^[0-9]+$ ]] || return 0
    address="$(address_for_index "$choice")"
    [[ -n "$address" ]] || return 0

    hyprctl dispatch focuswindow "address:$address" >/dev/null 2>&1 || true
}

case "${1:-status}" in
    status) waybar_status ;;
    menu) open_menu ;;
    *)
        printf 'Uso: %s [status|menu]\n' "$0" >&2
        exit 2
        ;;
esac
