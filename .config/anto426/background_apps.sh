#!/usr/bin/env bash
set -uo pipefail
export PATH="$HOME/.config/anto426/bin:$PATH"

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
    notify-send "Background Dashboard" "$1" 2>/dev/null || true
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

ensure_icon_cache() {
    mkdir -p "$CACHE_DIR" 2>/dev/null
    [[ -f "$ICON_CACHE_FILE" ]] || touch "$ICON_CACHE_FILE"
}

desktop_icon_for_class() {
    local class="${1:-}"
    local class_lc
    class_lc="$(printf '%s' "$class" | tr '[:upper:]' '[:lower:]')"
    
    case "$class_lc" in
        waybar) printf 'utilities-system-monitor' ;;
        swaync) printf 'preferences-system-notifications' ;;
        cava) printf 'audio-volume-high' ;;
        vlc) printf 'vlc' ;;
        ghostty) printf 'terminal' ;;
        python*) printf 'python' ;;
        *) printf 'application-x-executable' ;;
    esac
}

get_running_processes() {
    ps -u "$USER" -o pid,ppid,%cpu,%mem,comm,args --no-headers 2>/dev/null | awk '
        {
            pid = $1
            ppid = $2
            cpu = $3
            mem = $4
            comm = $5
            args = $0
            sub(/^[[:space:]]*[0-9]+[[:space:]]+[0-9]+[[:space:]]+[0-9.]+[[:space:]]+[0-9.]+[[:space:]]+[^[:space:]]+[[:space:]]+/, "", args)
            
            if (comm ~ /^(bash|zsh|sh|ps|awk|grep|sed|sleep|cat|sort|git|systemd|systemctl|ssh-agent|gnupg|dbus-broker|at-spi|gvfsd|dconf-service|\(sd-pam\)|ls|pgrep)$/ || pid == '$$') {
                next
            }
            if (comm == "Hyprland" || comm == "Xwayland") {
                next
            }
            printf "%s\t%s\t%s\t%s\t%s\t%s\n", pid, ppid, cpu, mem, comm, args
        }
    '
}

categorize_process() {
    local comm="$1"
    local args="$2"
    
    if [[ "$comm" =~ ^(waybar|swaync|awww-daemon|fcitx5|pipewire|wireplumber|pipewire-pulse|dunst|xdg-desktop-portal.*|dbus-broker.*)$ ]]; then
        printf 'daemon'
    elif [[ "$comm" =~ ^(cava|vlc|mpd|ghostty|alacritty|kitty|firefox|chromium|discord|spotify|telegram.*|steam.*|python.*|node.*|perl|ruby|go|rustc)$ ]]; then
        printf 'app'
    else
        if [[ "$args" == *"/"* || "$args" == *"."* ]]; then
            printf 'app'
        else
            printf 'other'
        fi
    fi
}

build_menu_files() {
    local active clients apps address class initial_class title workspace icon label count
    local win_lines=() win_data=()
    local daemon_lines=() daemon_data=()
    local app_lines=() app_data=()

    : >"$MAP_FILE"
    : >"$MENU_FILE"

    # 1. Gather GUI Windows
    active="$(active_address)"
    clients="$(clients_json)"
    apps="$(background_clients_json "$clients" "$active")"

    while IFS=$'\t' read -r address class initial_class title workspace; do
        [[ -n "$address" ]] || continue
        class="${class:-App}"
        title="${title:-Window}"
        workspace="${workspace:-?}"
        icon="$(desktop_icon_for_class "$class")"
        
        # Trim class and title for compact 430px wide layout
        local short_class="$class"
        ((${#short_class} > 10)) && short_class="${short_class:0:8}.."
        local short_title="$title"
        ((${#short_title} > 15)) && short_title="${short_title:0:13}.."
        
        label="[WS $workspace]  $short_class  •  $short_title"
        ((${#label} > 80)) && label="${label:0:77}..."
        
        win_lines+=("$label|$icon")
        win_data+=("$label"$'\t'"window"$'\t'"$address"$'\t'"$class"$'\t'"$title"$'\t'"$workspace")
    done < <(
        printf '%s' "$apps" |
            jq -r '.[] | [
                (.address // ""),
                (.class // "App"),
                (.initialClass // .class // "App"),
                ((.title // "") | gsub("[\t\n\r]"; " ")),
                ((.workspace.name // .workspace.id // "?") | tostring)
            ] | @tsv' 2>/dev/null
    )

    # 2. Gather Running Processes
    while IFS=$'\t' read -r pid ppid cpu mem comm args; do
        [[ -n "$pid" ]] || continue
        local cat icon_name display_line
        cat="$(categorize_process "$comm" "$args")"
        icon_name="$(desktop_icon_for_class "$comm")"
        
        # Trim command name for compact 430px wide layout
        local short_comm="$comm"
        ((${#short_comm} > 12)) && short_comm="${short_comm:0:10}.."
        
        display_line="$short_comm  [$pid]  CPU: $cpu%  MEM: $mem%"
        
        if [[ "$cat" == "daemon" ]]; then
            daemon_lines+=("$display_line|$icon_name")
            daemon_data+=("$display_line"$'\t'"process"$'\t'"$pid"$'\t'"$comm"$'\t'"$cpu"$'\t'"$mem"$'\t'"$args")
        else
            app_lines+=("$display_line|$icon_name")
            app_data+=("$display_line"$'\t'"process"$'\t'"$pid"$'\t'"$comm"$'\t'"$cpu"$'\t'"$mem"$'\t'"$args")
        fi
    done < <(get_running_processes)

    # 3. Write Quick Actions to map and menu
    printf 'Refresh Dashboard\taction\trefresh\n' >>"$MAP_FILE"
    printf 'Kill All Background Apps\taction\tkillall\n' >>"$MAP_FILE"
    printf 'Back\taction\tback\n' >>"$MAP_FILE"

    printf 'Refresh Dashboard\0icon\x1fview-refresh\n' >>"$MENU_FILE"
    printf 'Kill All Background Apps\0icon\x1fapplication-x-executable\n' >>"$MENU_FILE"

    # 4. Write GUI Windows
    local win_count=${#win_lines[@]}
    if (( win_count > 0 )); then
        for ((i=0; i<win_count; i++)); do
            local item="${win_lines[i]}"
            local lbl="${item%|*}"
            local icn="${item#*|}"
            printf '%s\n' "${win_data[i]}" >>"$MAP_FILE"
            printf '%s\0icon\x1f%s\n' "$lbl" "$icn" >>"$MENU_FILE"
        done
    fi

    # 5. Write Desktop Services
    local daemon_count=${#daemon_lines[@]}
    if (( daemon_count > 0 )); then
        for ((i=0; i<daemon_count; i++)); do
            local item="${daemon_lines[i]}"
            local lbl="${item%|*}"
            local icn="${item#*|}"
            printf '%s\n' "${daemon_data[i]}" >>"$MAP_FILE"
            printf '%s\0icon\x1f%s\n' "$lbl" "$icn" >>"$MENU_FILE"
        done
    fi

    # 6. Write User Background Processes
    local app_count=${#app_lines[@]}
    if (( app_count > 0 )); then
        for ((i=0; i<app_count; i++)); do
            local item="${app_lines[i]}"
            local lbl="${item%|*}"
            local icn="${item#*|}"
            printf '%s\n' "${app_data[i]}" >>"$MAP_FILE"
            printf '%s\0icon\x1f%s\n' "$lbl" "$icn" >>"$MENU_FILE"
        done
    fi

    printf 'Back\0icon\x1fgo-previous\n' >>"$MENU_FILE"
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

close_app() {
    local address="$1"
    [[ -n "$address" ]] || return 1
    hyprctl dispatch closewindow "address:$address" >/dev/null 2>&1
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

build_workspace_menu_files() {
    local id name label arg number
    local -a lines=()
    local -a args=()

    : >"$WORKSPACE_MAP_FILE"
    : >"$WORKSPACE_MENU_FILE"

    while IFS=$'\t' read -r id name; do
        [[ -n "$id" ]] || continue
        label="$(workspace_label_from_name "$id" "$name")"
        arg="$(workspace_arg_from_name "$id" "$name")"
        workspace_label_exists "$label" && continue
        lines+=("$label")
        args+=("$arg")
    done < <(
        hyprctl workspaces -j 2>/dev/null |
            jq -r 'sort_by(.id)[] | [(.id // empty | tostring), (.name // empty)] | @tsv' 2>/dev/null
    )

    for number in {1..10}; do
        label="Workspace $number"
        arg="$number"
        workspace_label_exists "$label" && continue
        lines+=("$label")
        args+=("$arg")
    done

    local count=${#lines[@]}
    if (( count > 0 )); then
        for ((i=0; i<count; i++)); do
            label="${lines[i]}"
            arg="${args[i]}"
            printf '%s\t%s\n' "$label" "$arg" >>"$WORKSPACE_MAP_FILE"
            printf '%s\0icon\x1fview-grid-symbolic\n' "$label" >>"$WORKSPACE_MENU_FILE"
        done
    else
        printf 'No workspaces available\0icon\x1fdialog-error\n' >>"$WORKSPACE_MENU_FILE"
    fi
    printf 'Back\0icon\x1fgo-previous\n' >>"$WORKSPACE_MENU_FILE"
    printf 'Back\tback\n' >>"$WORKSPACE_MAP_FILE"
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
    if [[ "$choice" == "Back" ]]; then
        return 0
    fi

    workspace="$(awk -F'\t' -v label="$choice" '$1 == label { print $2; exit }' "$WORKSPACE_MAP_FILE" 2>/dev/null)"
    [[ -n "$workspace" ]] || return 0
    hyprctl dispatch movetoworkspacesilent "$workspace,address:$address" >/dev/null 2>&1 &&
        notify "App moved successfully"
}

app_actions_menu() {
    local address="$1"
    local class="$2"
    local title="$3"
    local workspace="$4"
    local choice app_name

    app_name="$class"
    [[ -n "$title" ]] && app_name="$class: $title"
    
    local short_name="$app_name"
    ((${#short_name} > 30)) && short_name="${short_name:0:27}..."

    while true; do
        choice="$(
            {
                printf 'Focus & Raise Window\0icon\x1fwindow-restore\n'
                printf 'Bring Window Here\0icon\x1fgo-home\n'
                printf 'Send Window to Workspace...\0icon\x1fview-grid-symbolic\n'
                printf 'Close Window\0icon\x1fwindow-close\n'
                printf 'Back\0icon\x1fgo-previous\n'
            } |
                rofi -dmenu -i -matching fuzzy \
                    -p "$short_name" \
                    -theme "$THEME"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == "Back" ]]; then
            return 2
        fi

        case "$choice" in
            "Focus & Raise Window")
                focus_app "$address" || notify "Could not open window"
                return 0
                ;;
            "Bring Window Here")
                move_app_to_current "$address" || notify "Could not bring window here"
                return 0
                ;;
            "Send Window to Workspace...")
                move_app_to_workspace_menu "$address"
                return 0
                ;;
            "Close Window")
                close_app "$address" && notify "Close request sent"
                return 0
                ;;
        esac
    done
}

restart_process() {
    local comm="$1"
    local pid="$2"
    local args="$3"
    
    notify "Restarting $comm..."
    kill -15 "$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null
    sleep 0.4
    
    if [[ "$comm" == "waybar" ]]; then
        waybar &
    elif [[ "$comm" == "swaync" ]]; then
        swaync &
    elif [[ -n "$args" ]]; then
        eval "$args" &
    else
        $comm &
    fi
    notify "$comm restarted"
}

process_actions_menu() {
    local pid="$1"
    local comm="$2"
    local cpu="$3"
    local mem="$4"
    local args="$5"
    local choice ppid

    ppid="$(ps -p "$pid" -o ppid= 2>/dev/null | tr -d ' ' || printf '1')"
    
    local short_comm="$comm"
    ((${#short_comm} > 24)) && short_comm="${short_comm:0:21}..."

    while true; do
        choice="$(
            {
                printf 'Show Child Processes\0icon\x1fpreferences-system\n'
                printf 'Terminate Process (SIGTERM)\0icon\x1fprocess-stop\n'
                printf 'Force Kill Process (SIGKILL)\0icon\x1fprocess-stop\n'
                printf 'Relaunch & Restart Process\0icon\x1fview-refresh\n'
                printf 'Back\0icon\x1fgo-previous\n'
            } |
                rofi -dmenu -i -matching fuzzy \
                    -p "$short_comm" \
                    -theme "$THEME"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == "Back" ]]; then
            return 2
        fi

        case "$choice" in
            "Show Child Processes")
                subprocesses_menu "$pid" "$comm"
                ;;
            "Terminate Process (SIGTERM)")
                if kill -15 "$pid" 2>/dev/null; then
                    notify "Sent SIGTERM to PID $pid ($comm)"
                else
                    notify "Failed to terminate PID $pid"
                fi
                return 0
                ;;
            "Force Kill Process (SIGKILL)")
                if kill -9 "$pid" 2>/dev/null; then
                    notify "Sent SIGKILL to PID $pid ($comm)"
                else
                    notify "Failed to force kill PID $pid"
                fi
                return 0
                ;;
            "Relaunch & Restart Process")
                restart_process "$comm" "$pid" "$args"
                return 0
                ;;
        esac
    done
}

get_process_tree() {
    local parent="$1"
    local level="${2:-0}"
    local children=()
    mapfile -t children < <(pgrep -P "$parent" 2>/dev/null)
    local count=${#children[@]}
    local i child comm cpu mem next_level

    for ((i=0; i<count; i++)); do
        child="${children[i]}"
        [[ -n "$child" ]] || continue
        comm="$(ps -p "$child" -o comm= 2>/dev/null || printf '?')"
        cpu="$(ps -p "$child" -o %cpu= 2>/dev/null | tr -d ' ' || printf '0')"
        mem="$(ps -p "$child" -o %mem= 2>/dev/null | tr -d ' ' || printf '0')"
        
        next_level=$((level + 1))

        local indent=""
        local j
        for ((j=0; j<level; j++)); do
            indent="${indent}    "
        done
        
        # Dynamically scale command length to fit nested tree
        local short_comm="$comm"
        local max_comm_len=$((14 - (level * 2)))
        ((max_comm_len < 6)) && max_comm_len=6
        ((${#short_comm} > max_comm_len)) && short_comm="${short_comm:0:$((max_comm_len-2))}.."
        
        printf '%s%s  [%s]  CPU: %s%%  MEM: %s%%\t%s\t%s\n' "$indent" "$short_comm" "$child" "$cpu" "$mem" "$child" "$comm"
        get_process_tree "$child" "$next_level"
    done
}

subprocesses_menu() {
    local parent_pid="$1"
    local parent_comm="$2"
    local choice child_pid child_comm child_action clean_action
    
    while true; do
        local tree_map=()
        local tree_lines=()
        
        while IFS=$'\t' read -r line pid comm; do
            [[ -n "$line" ]] || continue
            tree_lines+=("$line")
            tree_map+=("$line"$'\t'"$pid"$'\t'"$comm")
        done < <(get_process_tree "$parent_pid")
        
        local tree_count=${#tree_lines[@]}
        
        choice="$(
            {
                if (( tree_count > 0 )); then
                    for tl in "${tree_lines[@]}"; do
                        printf '%s\0icon\x1fapplication-x-executable\n' "$tl"
                    done
                else
                    printf 'No active child processes\0icon\x1finfo\n'
                fi
                printf 'Back\0icon\x1fgo-previous\n'
            } |
                rofi -dmenu -i -p "${parent_comm:0:15} (Children)" \
                    -theme "$THEME"
        )"
        
        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == "Back" ]]; then
            return 0
        fi
        
        local stripped_choice="$choice"
        
        child_pid=""
        child_comm=""
        for tm in "${tree_map[@]}"; do
            local tm_line tm_pid tm_comm
            tm_line="$(printf '%s' "$tm" | cut -f1)"
            tm_pid="$(printf '%s' "$tm" | cut -f2)"
            tm_comm="$(printf '%s' "$tm" | cut -f3)"
            
            if [[ "$tm_line" == "$stripped_choice" ]]; then
                child_pid="$tm_pid"
                child_comm="$tm_comm"
                break
            fi
        done
        
        [[ -n "$child_pid" ]] || continue
        
        while true; do
            child_action="$(
                {
                    printf 'Terminate Process (SIGTERM)\0icon\x1fprocess-stop\n'
                    printf 'Force Kill Process (SIGKILL)\0icon\x1fprocess-stop\n'
                    printf 'Back\0icon\x1fgo-previous\n'
                } |
                    rofi -dmenu -i -p "${child_comm:0:15} [$child_pid]" \
                        -theme "$THEME"
            )"
            
            [[ -z "$child_action" ]] && break
            if [[ "$child_action" == "Back" ]]; then
                break
            fi

            case "$child_action" in
                "Terminate Process (SIGTERM)")
                    kill -15 "$child_pid" 2>/dev/null && notify "Terminated PID $child_pid"
                    break 2
                    ;;
                "Force Kill Process (SIGKILL)")
                    kill -9 "$child_pid" 2>/dev/null && notify "Killed PID $child_pid"
                    break 2
                    ;;
            esac
        done
    done
}

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    return 0
}

open_menu() {
    local choice row type address class title workspace pid comm cpu mem args
    local cpu_tot mem_tot win_count proc_tot message_card

    require_stack || {
        notify "Hyprland or jq not available"
        return 1
    }

    pkill -x rofi 2>/dev/null || true

    while true; do
        build_menu_files

        cpu_tot="$(ps -u "$USER" -o %cpu | awk '{s+=$1} END {printf "%.1f", s}')"
        mem_tot="$(ps -u "$USER" -o %mem | awk '{s+=$1} END {printf "%.1f", s}')"
        win_count="$(background_clients_json "$(clients_json)" "$(active_address)" | jq 'length' 2>/dev/null || printf '0')"
        proc_tot="$(get_running_processes | wc -l || printf '0')"
        
        choice="$(
            rofi -dmenu -i -matching fuzzy -show-icons \
                -p "Background Apps ($((win_count + proc_tot)))" \
                -theme "$THEME" <"$MENU_FILE"
        )"

        [[ -n "$choice" ]] || return 0
        local clean_choice="$choice"
        
        if [[ "$clean_choice" == "Back" ]]; then
            go_back
            return 0
        fi
        
        row="$(awk -F'\t' -v label="$clean_choice" '$1 == label { print; exit }' "$MAP_FILE" 2>/dev/null)"
        
        if [[ -z "$row" ]]; then
            if [[ "$clean_choice" == "Refresh Dashboard" ]]; then
                continue
            fi
            if [[ "$clean_choice" == "Kill All Background Apps" ]]; then
                while IFS=$'\t' read -r pid _ _ _ comm args; do
                    [[ -n "$pid" ]] || continue
                    if [[ "$(categorize_process "$comm" "$args")" == "app" ]]; then
                        kill -15 "$pid" 2>/dev/null || kill -9 "$pid" 2>/dev/null
                    fi
                done < <(get_running_processes)
                notify "All background apps terminated"
                continue
            fi
            continue
        fi
        
        IFS=$'\t' read -r _ type address class title workspace args <<<"$row"

        case "$type" in
            window)
                app_actions_menu "$address" "$class" "$title" "$workspace"
                ;;
            process)
                pid="$address"
                comm="$class"
                cpu="$title"
                mem="$workspace"
                process_actions_menu "$pid" "$comm" "$cpu" "$mem" "$args"
                ;;
        esac
    done
}

waybar_status() {
    local active clients apps count tooltip_str class text
    
    if ! require_stack; then
        printf '{"text":"󰘔","tooltip":"Hyprland/jq non disponibile","class":"missing"}\n'
        return 0
    fi
    
    active="$(active_address)"
    clients="$(clients_json)"
    apps="$(background_clients_json "$clients" "$active")"
    
    local win_count
    win_count="$(printf '%s' "$apps" | jq -r 'length' 2>/dev/null || printf '0')"
    [[ "$win_count" =~ ^[0-9]+$ ]] || win_count=0
    
    local proc_count=0
    local proc_lines=()
    while IFS=$'\t' read -r pid ppid cpu mem comm args; do
        [[ -n "$pid" ]] || continue
        local cat
        cat="$(categorize_process "$comm" "$args")"
        if [[ "$cat" == "app" ]]; then
            ((proc_count++))
            proc_lines+=("• $comm [PID: $pid] — CPU: $cpu%, MEM: $mem%")
        fi
    done < <(get_running_processes)
    
    local total_count=$((win_count + proc_count))
    
    local tooltip_lines=()
    tooltip_lines+=("Dashboard Background")
    tooltip_lines+=("──────────────────")
    
    if ((win_count > 0)); then
        tooltip_lines+=("Applications / Windows:")
        while IFS=$'\t' read -r _ class _ title workspace; do
            [[ -n "$class" ]] || continue
            tooltip_lines+=("• $class — $title [$workspace]")
        done < <(
            printf '%s' "$apps" |
                jq -r '.[] | [
                    (.address // ""),
                    (.class // "App"),
                    (.initialClass // .class // "App"),
                    ((.title // "") | gsub("[\t\n\r]"; " ")),
                    ((.workspace.name // .workspace.id // "?") | tostring)
                ] | @tsv' 2>/dev/null
        )
    else
        tooltip_lines+=("No active GUI windows in background")
    fi
    
    tooltip_lines+=("")
    if ((proc_count > 0)); then
        tooltip_lines+=("Running background apps:")
        for pl in "${proc_lines[@]}"; do
            tooltip_lines+=("$pl")
        done
    else
        tooltip_lines+=("No CLI apps in background")
    fi
    
    tooltip_str="$(printf '%s\n' "${tooltip_lines[@]}")"
    
    class="empty"
    text="󰘔"
    if ((total_count > 0)); then
        class="active"
        text="󰘔 ${total_count}"
    fi
    
    printf '{"text":"%s","tooltip":"%s","class":"%s"}\n' \
        "$(json_escape "$text")" \
        "$(json_escape "$tooltip_str")" \
        "$class"
}

case "${1:-status}" in
    status) waybar_status ;;
    menu) open_menu ;;
    *)
        printf 'Uso: %s [status|menu]\n' "$0" >&2
        exit 2
        ;;
esac
