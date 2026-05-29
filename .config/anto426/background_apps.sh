#!/usr/bin/env bash
set -uo pipefail

THEME="${ROFI_BACKGROUND_APPS_THEME:-$HOME/.config/rofi/control_menu.rasi}"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}"
CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/anto426"
MAP_FILE="$RUNTIME_DIR/anto426-bg-apps.$$.map"
MENU_FILE="$RUNTIME_DIR/anto426-bg-apps.$$.menu"
WORKSPACE_MAP_FILE="$RUNTIME_DIR/anto426-bg-workspaces.$$.map"
WORKSPACE_MENU_FILE="$RUNTIME_DIR/anto426-bg-workspaces.$$.menu"
ICON_CACHE_FILE="$CACHE_DIR/bg-app-icons.tsv"
ICON_CACHE_MAX_AGE=86400

cleanup_menu_files() {
    rm -f "$MAP_FILE" "$MENU_FILE" "$WORKSPACE_MAP_FILE" "$WORKSPACE_MENU_FILE"
}

trap cleanup_menu_files EXIT

get_color() {
    local name="$1"
    local default="$2"
    local val key

    key="$name"
    val="$(
        awk -v key="$key" '
            $1 == "@define-color" && $2 == key {
                gsub(/;/, "", $3)
                print $3
                exit
            }
        ' "$HOME/.config/colors/colors.css" 2>/dev/null
    )"

    if [[ "$val" == "@"* ]]; then
        key="${val#@}"
        val="$(
            awk -v key="$key" '
                $1 == "@define-color" && $2 == key {
                    gsub(/;/, "", $3)
                    print $3
                    exit
                }
            ' "$HOME/.config/colors/colors.css" 2>/dev/null
        )"
    fi

    printf '%s' "${val:-$default}"
}

c_accent="$(get_color accent "#8cb8e4")"
c_muted="$(get_color muted "#b9c4d2")"
c_yellow="$(get_color yellow "#f9e2af")"
c_green="$(get_color green "#a6e3a1")"
c_red="$(get_color red "#f38ba8")"
c_cyan="$(get_color cyan "#89dceb")"

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

notify() {
    notify-send "App background" "$1" 2>/dev/null || true
}

clients_json() {
    hyprctl clients -j 2>/dev/null || printf '[]'
}

active_address() {
    hyprctl activewindow -j 2>/dev/null |
        jq -r '.address // empty' 2>/dev/null ||
        printf ''
}

current_workspace_arg() {
    local json name id

    json="$(hyprctl activeworkspace -j 2>/dev/null)" || return 1
    name="$(printf '%s' "$json" | jq -r '.name // empty' 2>/dev/null)"
    id="$(printf '%s' "$json" | jq -r '.id // empty' 2>/dev/null)"

    if [[ -z "$name" ]]; then
        [[ -n "$id" ]] && printf '%s' "$id"
        return 0
    fi

    if [[ "$name" =~ ^[0-9]+$ || "$name" == special:* ]]; then
        printf '%s' "$name"
    else
        printf 'name:%s' "$name"
    fi
}

background_clients_filter='
    map(select(
        (.mapped // true) == true
        and (.hidden // false) == false
        and ((.address // "") != $active_address)
        and ((.class // .initialClass // "") | test("^(rofi|waybar|swaync|swaync-control-center|wofi|anto426-osd)$"; "i") | not)
        and ((.title // "") | length > 0)
    ))
'

background_clients_json() {
    local clients="$1"
    local active="$2"

    printf '%s' "$clients" |
        jq -c --arg active_address "$active" "$background_clients_filter |
            sort_by((.workspace.id // 9999), (.class // .initialClass // \"\"), (.title // \"\"))" 2>/dev/null ||
        printf '[]'
}

app_count() {
    local active clients
    active="$(active_address)"
    clients="$(clients_json)"
    background_clients_json "$clients" "$active" | jq -r 'length' 2>/dev/null || printf '0'
}

app_tooltip() {
    local active clients apps
    active="$(active_address)"
    clients="$(clients_json)"
    apps="$(background_clients_json "$clients" "$active")"
    printf '%s' "$apps" |
        jq -r '
            if length == 0 then
                "Nessuna app in background"
            else
                ["App in background"] +
                ([.[0:12][] |
                    "• " + ((.class // .initialClass // "App") | tostring) +
                    " — " + ((.title // "") | tostring) +
                    " [" + ((.workspace.name // .workspace.id // "?") | tostring) + "]"
                ]) +
                (if length > 12 then ["• ..."] else [] end) |
                join("\n")
            end' 2>/dev/null ||
        printf 'App in background'
}

waybar_status() {
    local active clients apps count tooltip class text

    if ! require_stack; then
        printf '{"text":"󰘔","tooltip":"Hyprland/jq non disponibile","class":"missing"}\n'
        return 0
    fi

    active="$(active_address)"
    clients="$(clients_json)"
    apps="$(background_clients_json "$clients" "$active")"
    count="$(printf '%s' "$apps" | jq -r 'length' 2>/dev/null || printf '0')"
    [[ "$count" =~ ^[0-9]+$ ]] || count=0
    tooltip="$(
        printf '%s' "$apps" |
            jq -r '
                if length == 0 then
                    "Nessuna app in background"
                else
                    ["App in background"] +
                    ([.[0:12][] |
                        "• " + ((.class // .initialClass // "App") | tostring) +
                        " — " + ((.title // "") | tostring) +
                        " [" + ((.workspace.name // .workspace.id // "?") | tostring) + "]"
                    ]) +
                    (if length > 12 then ["• ..."] else [] end) |
                    join("\n")
                end' 2>/dev/null ||
            printf 'App in background'
    )"
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

icon_cache_is_fresh() {
    local now mtime

    [[ -f "$ICON_CACHE_FILE" ]] || return 1
    now="$(date +%s)"
    mtime="$(stat -c %Y "$ICON_CACHE_FILE" 2>/dev/null || printf '0')"
    [[ "$mtime" =~ ^[0-9]+$ ]] || return 1
    ((now - mtime < ICON_CACHE_MAX_AGE))
}

rebuild_icon_cache() {
    local tmp file base_lc metadata startup icon name_lc

    mkdir -p "$CACHE_DIR" 2>/dev/null || return 0
    tmp="$ICON_CACHE_FILE.$$"
    : >"$tmp" || return 0

    while IFS= read -r -d '' file; do
        base_lc="$(basename "$file" .desktop | tr '[:upper:]' '[:lower:]')"
        metadata="$(
            awk -F= '
                tolower($1) == "startupwmclass" { startup = tolower($2) }
                tolower($1) == "icon" { icon = $2 }
                tolower($1) == "name" && name == "" { name = tolower($2) }
                END {
                    gsub(/^[ \t]+|[ \t]+$/, "", startup)
                    gsub(/^[ \t]+|[ \t]+$/, "", icon)
                    gsub(/^[ \t]+|[ \t]+$/, "", name)
                    print startup "\t" icon "\t" name
                }
            ' "$file" 2>/dev/null
        )"
        IFS=$'\t' read -r startup icon name_lc <<<"$metadata"
        [[ -n "$icon" ]] || continue

        printf '%s\t%s\n' "$base_lc" "$icon" >>"$tmp"
        [[ -n "$startup" ]] && printf '%s\t%s\n' "$startup" "$icon" >>"$tmp"
        [[ -n "$name_lc" ]] && printf '%s\t%s\n' "$name_lc" "$icon" >>"$tmp"
    done < <(
        find \
            "$HOME/.local/share/applications" \
            "$HOME/.local/share/flatpak/exports/share/applications" \
            /var/lib/flatpak/exports/share/applications \
            /usr/local/share/applications \
            /usr/share/applications \
            -maxdepth 1 -type f -name '*.desktop' -print0 2>/dev/null
    )

    awk -F'\t' 'NF >= 2 && $1 != "" && $2 != "" && !seen[$1]++ { print $1 "\t" $2 }' "$tmp" >"$tmp.dedup" 2>/dev/null &&
        mv "$tmp.dedup" "$ICON_CACHE_FILE"
    rm -f "$tmp" "$tmp.dedup"
}

ensure_icon_cache() {
    icon_cache_is_fresh || rebuild_icon_cache
}

desktop_icon_for_class() {
    local class="${1:-}"
    local initial_class="${2:-}"
    local class_lc initial_lc icon

    class_lc="$(printf '%s' "$class" | tr '[:upper:]' '[:lower:]')"
    initial_lc="$(printf '%s' "$initial_class" | tr '[:upper:]' '[:lower:]')"

    ensure_icon_cache
    icon="$(
        awk -F'\t' -v class="$class_lc" -v initial="$initial_lc" '
            ($1 == class || $1 == initial) && $2 != "" { print $2; exit }
        ' "$ICON_CACHE_FILE" 2>/dev/null
    )"

    printf '%s' "${icon:-${class_lc:-application-x-executable}}"
}

single_line() {
    printf '%s' "${1:-}" | tr '\t\r\n' '   ' | sed -e 's/  */ /g' -e 's/^ //' -e 's/ $//'
}

short_address() {
    local raw="${1#0x}"
    if ((${#raw} > 6)); then
        printf '%s' "${raw: -6}"
    else
        printf '%s' "$raw"
    fi
}

label_exists() {
    local label="$1"
    awk -F'\t' -v label="$label" '$1 == label { found = 1 } END { exit found ? 0 : 1 }' "$MAP_FILE" 2>/dev/null
}

build_menu_files() {
    local active clients apps address class initial_class title workspace icon label short count

    : >"$MAP_FILE"
    : >"$MENU_FILE"
    ensure_icon_cache

    active="$(active_address)"
    clients="$(clients_json)"
    apps="$(background_clients_json "$clients" "$active")"

    # Action section
    printf '%s\t%s\t\t\t\t\n' "Refresh list" "refresh" >>"$MAP_FILE"
    
    printf '󰑓 QUICK ACTIONS\n' >>"$MENU_FILE"
    printf ' └─ 󰑓 Refresh list\0icon\x1fview-refresh\n' >>"$MENU_FILE"

    printf '\n󰘔 BACKGROUND APPLICATIONS\n' >>"$MENU_FILE"

    local app_lines=()
    local app_data=()
    
    while IFS=$'\t' read -r address class initial_class title workspace; do
        [[ -n "$address" ]] || continue
        class="$(single_line "$class")"
        initial_class="$(single_line "$initial_class")"
        title="$(single_line "$title")"
        workspace="$(single_line "$workspace")"
        icon="$(desktop_icon_for_class "$class" "$initial_class")"
        label="${class}"
        [[ -n "$title" ]] && label="${label}  ${title}"
        label="${label}  [${workspace}]"
        
        # Check uniqueness
        if label_exists "$label"; then
            short="$(short_address "$address")"
            label="${label}  #${short}"
        fi
        
        app_lines+=("$label|$icon")
        app_data+=("$label"$'\t'"app"$'\t'"$address"$'\t'"$class"$'\t'"$title"$'\t'"$workspace")
    done < <(
        printf '%s' "$apps" |
            jq -r '
                .[] | [
                    (.address // ""),
                    (.class // "App"),
                    (.initialClass // .class // "App"),
                    ((.title // "") | gsub("[\t\n\r]"; " ")),
                    ((.workspace.name // .workspace.id // "?") | tostring)
                ] | @tsv'
    )

    local count=${#app_lines[@]}
    if (( count > 0 )); then
        for ((i=0; i<count; i++)); do
            local item="${app_lines[i]}"
            local lbl="${item%|*}"
            local icn="${item#*|}"
            
            local data="${app_data[i]}"
            printf '%s\n' "$data" >>"$MAP_FILE"
            
            if (( i == count - 1 )); then
                printf ' └─ %s\0icon\x1f%s\n' "$lbl" "$icn" >>"$MENU_FILE"
            else
                printf ' ├─ %s\0icon\x1f%s\n' "$lbl" "$icn" >>"$MENU_FILE"
            fi
        done
    else
        printf ' └─ 󰂲 No active applications\0icon\x1fapplication-x-executable\n' >>"$MENU_FILE"
    fi

    ((count > 0))
}

focus_app() {
    local address="$1"
    [[ -n "$address" ]] || return 1
    hyprctl dispatch focuswindow "address:$address" >/dev/null 2>&1
}

move_app_to_current() {
    local address="$1"
    local workspace

    workspace="$(current_workspace_arg)"
    [[ -n "$workspace" ]] || return 1
    hyprctl dispatch movetoworkspacesilent "$workspace,address:$address" >/dev/null 2>&1 &&
        focus_app "$address"
}

workspace_arg_from_name() {
    local id="$1"
    local name="$2"

    if [[ -z "$name" ]]; then
        printf '%s' "$id"
    elif [[ "$name" =~ ^[0-9]+$ || "$name" == special:* ]]; then
        printf '%s' "$name"
    else
        printf 'name:%s' "$name"
    fi
}

workspace_label_from_name() {
    local id="$1"
    local name="$2"

    if [[ "$name" == special:* ]]; then
        printf 'Special workspace %s' "${name#special:}"
    elif [[ -n "$name" ]]; then
        printf 'Workspace %s' "$name"
    else
        printf 'Workspace %s' "$id"
    fi
}

workspace_label_exists() {
    local label="$1"
    awk -F'\t' -v label="$label" '$1 == label { found = 1 } END { exit found ? 0 : 1 }' "$WORKSPACE_MAP_FILE" 2>/dev/null
}

add_workspace_option() {
    local label="$1"
    local arg="$2"
    local icon="${3:-view-grid-symbolic}"

    [[ -n "$label" && -n "$arg" ]] || return 0
    workspace_label_exists "$label" && return 0
    printf '%s\t%s\n' "$label" "$arg" >>"$WORKSPACE_MAP_FILE"
    printf '%s\0icon\x1f%s\n' "$label" "$icon" >>"$WORKSPACE_MENU_FILE"
}

build_workspace_menu_files() {
    local id name label arg number

    : >"$WORKSPACE_MAP_FILE"
    : >"$WORKSPACE_MENU_FILE"

    hyprctl workspaces -j 2>/dev/null |
        jq -r 'sort_by(.id)[] | [(.id // empty | tostring), (.name // empty)] | @tsv' 2>/dev/null |
        while IFS=$'\t' read -r id name; do
            label="$(workspace_label_from_name "$id" "$name")"
            arg="$(workspace_arg_from_name "$id" "$name")"
            add_workspace_option "$label" "$arg"
        done

    for number in {1..10}; do
        add_workspace_option "Workspace $number" "$number"
    done
}

move_app_to_workspace_menu() {
    local address="$1"
    local choice workspace

    build_workspace_menu_files
    choice="$(
        rofi -dmenu -i -matching fuzzy -show-icons \
            -p "Move where?" \
            -theme "$THEME" <"$WORKSPACE_MENU_FILE"
    )"
    [[ -n "$choice" ]] || return 0
    workspace="$(awk -F'\t' -v label="$choice" '$1 == label { print $2; exit }' "$WORKSPACE_MAP_FILE" 2>/dev/null)"
    [[ -n "$workspace" ]] || return 0
    hyprctl dispatch movetoworkspacesilent "$workspace,address:$address" >/dev/null 2>&1 &&
        notify "App moved"
}

close_app() {
    local address="$1"
    [[ -n "$address" ]] || return 1
    hyprctl dispatch closewindow "address:$address" >/dev/null 2>&1
}

app_actions_menu() {
    local address="$1"
    local class="$2"
    local title="$3"
    local workspace="$4"
    local prompt action app_name

    app_name="$class"
    [[ -n "$title" ]] && app_name="$class: $title"
    prompt="$app_name"
    ((${#prompt} > 42)) && prompt="${prompt:0:39}..."

    local message_card
    message_card="Class: <b><span color='${c_accent}'>${class}</span></b>\nCurrent workspace: <b><span color='${c_yellow}'>${workspace:-?}</span></b>"

    action="$(
        {
            printf '󰘔 APPLICATION CONTROLS\n'
            printf ' ├─ 󰍉 Open window\n'
            printf ' ├─ 󰁝 Bring here (Move)\n'
            printf ' ├─ 󰏫 Move to workspace...\n'
            printf ' └─ 󰅖 Close window\n'
            
            printf '\n󰌍 NAVIGATION\n'
            printf ' └─ 󰌑 Back\n'
        } |
            rofi -dmenu -i -matching fuzzy \
                -p "Manage" \
                -mesg "$message_card" \
                -theme "$THEME"
    )"

    [[ -z "$action" ]] && return 0
    if [[ "$action" != *"├─ "* && "$action" != *"└─ "* ]]; then
        app_actions_menu "$address" "$class" "$title" "$workspace"
        return $?
    fi
    local clean_action
    clean_action="$(printf '%s' "$action" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

    case "$clean_action" in
        *"Apri"* | *"Open"*)
            focus_app "$address" || notify "Could not open app"
            ;;
        *"Porta qui"* | *"Bring here"*)
            move_app_to_current "$address" || notify "Could not bring here"
            ;;
        *"Sposta in spazio"* | *"Move to workspace"*)
            move_app_to_workspace_menu "$address"
            ;;
        *"Chiudi"* | *"Close"*)
            close_app "$address" && notify "Close request sent"
            ;;
        *"Indietro"* | *"Back"*)
            return 2
            ;;
    esac
}

open_menu() {
    local choice row type address class title workspace count

    require_stack || {
        notify "Hyprland or jq not available"
        return 1
    }

    pkill -x rofi 2>/dev/null || true

    while true; do
        if ! build_menu_files; then
            notify "No apps in background"
            return 0
        fi

        count="$(awk -F'\t' '$2 == "app" { count++ } END { print count + 0 }' "$MAP_FILE" 2>/dev/null)"
        choice="$(
            rofi -dmenu -i -matching fuzzy -show-icons \
                -p "Background Apps ($count)" \
                -theme "$THEME" <"$MENU_FILE"
        )"

        [[ -n "$choice" ]] || return 0
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        row="$(awk -F'\t' -v label="$clean_choice" '$1 == label { print; exit }' "$MAP_FILE" 2>/dev/null)"
        [[ -n "$row" ]] || return 0
        IFS=$'\t' read -r _ type address class title workspace _ <<<"$row"

        case "$type" in
            refresh)
                continue
                ;;
            app)
                app_actions_menu "$address" "$class" "$title" "$workspace"
                [[ $? -eq 2 ]] && continue
                return 0
                ;;
        esac
    done

}

case "${1:-status}" in
    status) waybar_status ;;
    menu) open_menu ;;
    *)
        printf 'Uso: %s [status|menu]\n' "$0" >&2
        exit 2
        ;;
esac
