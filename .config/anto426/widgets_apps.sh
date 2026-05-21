#!/usr/bin/env bash

custom_widgets_file() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets_custom.env"
}

custom_widgets_dir() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets.d"
}

custom_widget_env_file() {
    printf '%s/%s.env' "$(custom_widgets_dir)" "$1"
}

ensure_custom_widgets_file() {
    local file
    file="$(custom_widgets_file)"
    mkdir -p "$(dirname "$file")"
    [[ -f "$file" ]] || cat >"$file" <<'EOF'
# Custom widgets list. Per-widget config lives in ~/.config/anto426/widgets.d/<id>.env
# Legacy lines in the form id|Name|StartupWMClass|exec command are still supported.
export ANTO426_CUSTOM_WIDGETS=""
EOF
}

custom_widget_ids() {
    local file value

    ensure_custom_widgets_file
    file="$(custom_widgets_file)"
    value="$(
        awk -F= '
            /^export[[:space:]]+ANTO426_CUSTOM_WIDGETS=/ {
                value = substr($0, index($0, "=") + 1)
                gsub(/^"/, "", value)
                gsub(/"$/, "", value)
                print value
                exit
            }
        ' "$file" 2>/dev/null
    )"
    printf '%s\n' ${value:-}
}

custom_widget_id_from_text() {
    local text="$1"
    local id

    id="$(printf '%s' "$text" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '_' | sed -e 's/^_*//' -e 's/_*$//')"
    [[ -n "$id" ]] || id="widget"
    printf '%s' "$id"
}

custom_widget_exists() {
    local id="$1"
    case " $(custom_widget_ids | tr '\n' ' ') " in
        *" $id "*) return 0 ;;
        *) return 1 ;;
    esac
}

custom_widget_unique_id() {
    local base id suffix

    base="$(custom_widget_id_from_text "$1")"
    id="$base"
    suffix=2
    while custom_widget_exists "$id"; do
        id="${base}_${suffix}"
        suffix=$((suffix + 1))
    done
    printf '%s' "$id"
}

custom_widget_list_add() {
    local id="$1"
    local file tmp

    ensure_custom_widgets_file
    file="$(custom_widgets_file)"
    tmp="$(mktemp)"
    awk -v id="$id" '
        BEGIN { found = 0 }
        /^export[[:space:]]+ANTO426_CUSTOM_WIDGETS=/ {
            value = substr($0, index($0, "=") + 1)
            gsub(/^"/, "", value)
            gsub(/"$/, "", value)
            if (value == "") ids = id
            else if (index(" " value " ", " " id " ") == 0) ids = value " " id
            else ids = value
            print "export ANTO426_CUSTOM_WIDGETS=\"" ids "\""
            found = 1
            next
        }
        { print }
        END {
            if (!found) print "export ANTO426_CUSTOM_WIDGETS=\"" id "\""
        }
    ' "$file" >"$tmp" && mv "$tmp" "$file"
}

custom_widget_list_remove() {
    local id="$1"
    local file tmp

    ensure_custom_widgets_file
    file="$(custom_widgets_file)"
    tmp="$(mktemp)"
    awk -F'|' -v id="$id" '
        $1 == id { next }
        /^export[[:space:]]+ANTO426_CUSTOM_WIDGETS=/ {
            value = substr($0, index($0, "=") + 1)
            gsub(/^"/, "", value)
            gsub(/"$/, "", value)
            count = split(value, old, " ")
            out = ""
            for (i = 1; i <= count; i++) {
                if (old[i] == "" || old[i] == id) continue
                out = out (out == "" ? "" : " ") old[i]
            }
            print "export ANTO426_CUSTOM_WIDGETS=\"" out "\""
            next
        }
        { print }
    ' "$file" >"$tmp" && mv "$tmp" "$file"
}

write_custom_widget_env() {
    local id="$1"
    local name="$2"
    local class="$3"
    local command="$4"
    local mode="${5:-app}"
    local width="${6:-520}"
    local height="${7:-360}"
    local monitor="${8:-}"
    local file

    mkdir -p "$(custom_widgets_dir)"
    file="$(custom_widget_env_file "$id")"
    {
        printf 'ANTO426_WIDGET_ID=%q\n' "$id"
        printf 'ANTO426_WIDGET_NAME=%q\n' "$name"
        printf 'ANTO426_WIDGET_CLASS=%q\n' "$class"
        printf 'ANTO426_WIDGET_COMMAND=%q\n' "$command"
        printf 'ANTO426_WIDGET_MODE=%q\n' "$mode"
        printf 'ANTO426_WIDGET_WIDTH=%q\n' "$width"
        printf 'ANTO426_WIDGET_HEIGHT=%q\n' "$height"
        printf 'ANTO426_WIDGET_MONITOR=%q\n' "$monitor"
    } >"$file"
}

legacy_custom_widget_line() {
    local id="$1"
    local file

    file="$(custom_widgets_file)"
    awk -F'|' -v id="$id" '$1 == id { print; exit }' "$file" 2>/dev/null
}

custom_widget_meta() {
    local id="$1"
    local field="$2"
    local file line name class cmd mode width height monitor

    ensure_custom_widgets_file
    file="$(custom_widget_env_file "$id")"
    if [[ -f "$file" ]]; then
        ANTO426_WIDGET_ID=""
        ANTO426_WIDGET_NAME=""
        ANTO426_WIDGET_CLASS=""
        ANTO426_WIDGET_COMMAND=""
        ANTO426_WIDGET_MODE=""
        ANTO426_WIDGET_WIDTH=""
        ANTO426_WIDGET_HEIGHT=""
        ANTO426_WIDGET_MONITOR=""
        # shellcheck disable=SC1090
        source "$file"
        name="$ANTO426_WIDGET_NAME"
        class="$ANTO426_WIDGET_CLASS"
        cmd="$ANTO426_WIDGET_COMMAND"
        mode="${ANTO426_WIDGET_MODE:-app}"
        width="${ANTO426_WIDGET_WIDTH:-520}"
        height="${ANTO426_WIDGET_HEIGHT:-360}"
        monitor="${ANTO426_WIDGET_MONITOR:-}"
    else
        line="$(legacy_custom_widget_line "$id")"
        [[ -n "$line" ]] || return 1

        name="$(printf '%s' "$line" | awk -F'|' '{print $2}')"
        class="$(printf '%s' "$line" | awk -F'|' '{print $3}')"
        cmd="$(printf '%s' "$line" | awk -F'|' '{print $4}')"
        mode="app"
        width="520"
        height="360"
        monitor=""
    fi

    case "$field" in
        id) printf '%s' "$id" ;;
        name) printf '%s' "$name" ;;
        class) printf '%s' "$class" ;;
        mode) printf '%s' "$mode" ;;
        title) printf '%s' "$name" ;;
        command) printf '%s' "$cmd" ;;
        w) printf '%s' "$width" ;;
        h) printf '%s' "$height" ;;
        monitor) printf '%s' "$monitor" ;;
    esac
}

custom_widget_regex() {
    printf '^%s$' "$(printf '%s' "$1" | sed -e 's/[.[\*^$()+?{}|\\]/\\&/g')"
}

pick_widget_monitor() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice monitor

    choice="$(
        {
            printf '%s\n' "Schermo attivo"
            if command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
                hyprctl monitors -j 2>/dev/null |
                    jq -r '.[] | "\(.name)  \(.width)x\(.height)  @ \(.x),\(.y)"'
            fi
        } | rofi -dmenu -i -matching fuzzy -p "Schermo widget" -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    if [[ "$choice" == "Schermo attivo" ]]; then
        monitor="$(focused_monitor_name 2>/dev/null || true)"
    else
        monitor="${choice%%  *}"
    fi

    printf '%s' "$monitor"
}

desktop_exec_line() {
    local desktop="$1"
    local exec_line

    exec_line="$(
        awk -F= '
            BEGIN { exec = "" }
            tolower($1) == "exec" && exec == "" { exec = $2 }
            END { print exec }
        ' "$desktop" 2>/dev/null
    )"
    [[ -n "$exec_line" ]] || return 1

    exec_line="$(
        printf '%s' "$exec_line" |
            sed -E 's/[[:space:]]+%[fFuUdDnNickvm]//g; s/%[fFuUdDnNickvm]//g'
    )"
    printf '%s' "$exec_line"
}

pretty_app_desktop_ids() {
    printf '%s\n' \
        spotify-launcher.desktop \
        zen.desktop \
        brave-browser.desktop \
        discord.desktop \
        org.telegram.desktop.desktop \
        steam.desktop \
        org.gnome.Loupe.desktop \
        vlc.desktop \
        mpv.desktop \
        gimp.desktop \
        nemo.desktop
}

find_desktop_file_by_id() {
    local desktop_id="$1"
    local dir

    for dir in "$HOME/.local/share/applications" /usr/share/applications; do
        [[ -f "$dir/$desktop_id" ]] && {
            printf '%s' "$dir/$desktop_id"
            return 0
        }
    done
    return 1
}

pretty_app_desktops() {
    local desktop_id desktop

    while IFS= read -r desktop_id; do
        [[ -n "$desktop_id" ]] || continue
        desktop="$(find_desktop_file_by_id "$desktop_id")" || continue
        printf '%s\n' "$desktop"
    done < <(pretty_app_desktop_ids)
}

pick_app_for_widget() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice desktop id name startup exec_line file monitor

    choice="$(
        pretty_app_desktops |
            while read -r desktop; do
                [[ "$(awk -F= '/^Type=/ {print tolower($2); exit}' "$desktop")" == "application" ]] || continue
                [[ "$(awk -F= '/^NoDisplay=/ {print tolower($2); exit}' "$desktop")" == "true" ]] && continue
                [[ "$(awk -F= '/^Hidden=/ {print tolower($2); exit}' "$desktop")" == "true" ]] && continue
                name="$(awk -F= '/^Name=/{print substr($0,index($0,$2)); exit}' "$desktop")"
                icon="$(awk -F= 'tolower($1)=="icon"{print $2; exit}' "$desktop")"
                [[ -n "$name" ]] || continue
                printf '%s\0icon\x1f%s\n' "$name" "${icon:-application-x-executable}"
            done |
            rofi -dmenu -i -matching fuzzy -show-icons -p "Aggiungi app carina" \
                -mesg "Lista filtrata: evita tool tecnici e launcher brutti" \
                -theme "$theme"
    )"

    [[ -n "$choice" ]] || return 1

    desktop="$(
        pretty_app_desktops |
            while read -r file; do
                name="$(awk -F= '/^Name=/{print substr($0,index($0,$2)); exit}' "$file")"
                [[ "$name" == "$choice" ]] && {
                    printf '%s' "$file"
                    exit 0
                }
            done
    )"
    [[ -n "$desktop" && -f "$desktop" ]] || return 1

    exec_line="$(desktop_exec_line "$desktop")" || return 1
    name="$(awk -F= '/^Name=/{print substr($0,index($0,$2)); exit}' "$desktop")"
    startup="$(
        awk -F= 'tolower($1) == "startupwmclass" {print $2; exit}' "$desktop" 2>/dev/null
    )"
    [[ -n "$startup" ]] || startup="$(basename "${desktop%.desktop}")"
    id="$(custom_widget_unique_id "$startup")"
    monitor="$(pick_widget_monitor)" || return 1

    write_custom_widget_env "$id" "$name" "$startup" "$exec_line" app 520 360 "$monitor"
    custom_widget_list_add "$id"
    write_custom_widget_hypr_rules
    printf '%s' "$id"
}

preset_widget_rows() {
    command -v cava >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰎈 Spettro audio" \
            "spettro_audio" \
            'cava -p "$HOME/.config/anto426/cava_widget.conf"' \
            620 220
    command -v btop >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰍛 Dashboard sistema" \
            "dashboard_sistema" \
            "btop" \
            760 520
    command -v fastfetch >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰌢 Scheda macchina" \
            "scheda_macchina" \
            'while true; do clear; image=$(find "$HOME/Pictures/neofetch" -type f \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.webp" \) 2>/dev/null | shuf -n 1); if [[ -n "$image" ]]; then fastfetch --logo "$image" --logo-type kitty --logo-width 18 --logo-padding-left 1 --logo-padding-right 2 --logo-preserve-aspect-ratio true --structure Title:Separator:OS:Kernel:Uptime:Packages:WM:CPU:Memory:Battery; else fastfetch --logo small --structure Title:Separator:OS:Kernel:Uptime:Packages:WM:CPU:Memory:Battery; fi; sleep 12; done' \
            620 320
    command -v asciiquarium >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰈺 Acquario ASCII" \
            "acquario_ascii" \
            "asciiquarium -t" \
            760 420
    command -v pipes.sh >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰕮 Tubi animati" \
            "tubi_animati" \
            "pipes.sh -t 2 -r 0 -R" \
            700 360
    command -v cbonsai >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰔱 Bonsai Zen" \
            "bonsai_zen" \
            "cbonsai -l -i -w 12 -L 38 -M 5" \
            540 380
    command -v cmatrix >/dev/null 2>&1 &&
        printf '%s|%s|%s|%s|%s\n' \
            "󰫐 Pioggia Matrix" \
            "pioggia_matrix" \
            "cmatrix -a -b -u 2" \
            620 360
}

pick_preset_widget() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice id_base command width height monitor id class

    choice="$(
        preset_widget_rows |
            awk -F'|' '{ print $1 }' |
            rofi -dmenu -i -matching fuzzy -p "Widget visuale" \
                -mesg "Preset controllati: belli da vedere e gia dimensionati" \
                -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    while IFS='|' read -r label id_base command width height; do
        [[ "$label" == "$choice" ]] || continue
        monitor="$(pick_widget_monitor)" || return 1
        id="$(custom_widget_unique_id "$id_base")"
        class="anto426.widget.cmd.$id"
        write_custom_widget_env "$id" "$label" "$class" "$command" terminal "$width" "$height" "$monitor"
        custom_widget_list_add "$id"
        write_custom_widget_hypr_rules
        printf '%s' "$id"
        return 0
    done < <(preset_widget_rows)

    return 1
}

prompt_terminal_widget_value() {
    local prompt="$1"
    local message="${2:-}"
    local theme="$HOME/.config/rofi/control_menu.rasi"

    rofi -dmenu -i -p "$prompt" -mesg "$message" -theme "$theme"
}

pick_terminal_widget() {
    local id name command width height class monitor

    name="$(prompt_terminal_widget_value "Nome widget" "Esempio: Processi, Log pacman, Temperatura")"
    [[ -n "$name" ]] || return 1

    command="$(prompt_terminal_widget_value "Comando" "Esempi: btop | watch -n1 sensors | journalctl -f")"
    [[ -n "$command" ]] || return 1

    width="$(prompt_terminal_widget_value "Larghezza" "Invio vuoto = 560")"
    height="$(prompt_terminal_widget_value "Altezza" "Invio vuoto = 320")"
    [[ "$width" =~ ^[0-9]+$ ]] || width=560
    [[ "$height" =~ ^[0-9]+$ ]] || height=320

    monitor="$(pick_widget_monitor)" || return 1
    id="$(custom_widget_unique_id "$name")"
    class="anto426.widget.cmd.$id"

    write_custom_widget_env "$id" "$name" "$class" "$command" terminal "$width" "$height" "$monitor"
    custom_widget_list_add "$id"
    write_custom_widget_hypr_rules
    printf '%s' "$id"
}

remove_custom_widget() {
    local id="$1"

    stop_custom_widget "$id" 2>/dev/null || true
    custom_widget_list_remove "$id"
    rm -f "$(custom_widget_env_file "$id")"
    write_custom_widget_hypr_rules
}

write_custom_widget_hypr_rules() {
    local file="$HOME/.config/hypr/conf/widget-apps.generated.conf"
    local id wm_class regex monitor x y w h

    {
        printf '# Generated by widgets_apps.sh - custom widget window rules\n'
        for id in $(custom_widget_ids); do
            [[ -n "$id" ]] || continue
            wm_class="$(custom_widget_meta "$id" class 2>/dev/null)" || continue
            regex="$(custom_widget_regex "$wm_class")"
            monitor="$(custom_widget_meta "$id" monitor 2>/dev/null || true)"
            w="$(custom_widget_meta "$id" w 2>/dev/null || printf '520')"
            h="$(custom_widget_meta "$id" h 2>/dev/null || printf '360')"
            x=""
            y=""
            if declare -F layout_var >/dev/null 2>&1; then
                x="$(layout_var "$id" x)"
                y="$(layout_var "$id" y)"
                w="$(layout_var "$id" w || printf '%s' "$w")"
                h="$(layout_var "$id" h || printf '%s' "$h")"
                [[ -n "$w" ]] || w="$(custom_widget_meta "$id" w 2>/dev/null || printf '520')"
                [[ -n "$h" ]] || h="$(custom_widget_meta "$id" h 2>/dev/null || printf '360')"
            fi
            printf 'windowrule = float on, match:class %s\n' "$regex"
            [[ -n "$monitor" ]] && printf 'windowrule = monitor %s silent, match:class %s\n' "$monitor" "$regex"
            printf 'windowrule = size %s %s, match:class %s\n' "$w" "$h" "$regex"
            [[ -n "$x" && -n "$y" ]] && printf 'windowrule = move %s %s, match:class %s\n' "$x" "$y" "$regex"
            printf 'windowrule = border_size 1, match:class %s\n' "$regex"
            printf 'windowrule = no_shadow on, match:class %s\n' "$regex"
            printf 'windowrule = no_anim on, match:class %s\n' "$regex"
            printf 'windowrule = no_initial_focus on, match:class %s\n' "$regex"
            printf 'windowrule = no_follow_mouse on, match:class %s\n' "$regex"
            printf 'windowrule = focus_on_activate off, match:class %s\n' "$regex"
            printf 'windowrule = rounding 14, match:class %s\n' "$regex"
            printf 'windowrule = opacity 0.94 0.88, match:class %s\n' "$regex"
        done
    } >"$file"

    hyprctl reload >/dev/null 2>&1 || true
}

launch_custom_widget() {
    local id="$1"
    local mode class title command monitor rules

    custom_widget_meta "$id" id >/dev/null || return 1
    write_custom_widget_hypr_rules
    mode="$(custom_widget_meta "$id" mode 2>/dev/null || printf 'app')"
    class="$(custom_widget_meta "$id" class)"
    title="$(custom_widget_meta "$id" title)"
    command="$(custom_widget_meta "$id" command)"
    monitor="$(custom_widget_meta "$id" monitor 2>/dev/null || true)"

    if [[ "$mode" == "terminal" ]]; then
        launch_widget "$id" "$class" "$title" "$command"
    else
        rules="float;size $(custom_widget_meta "$id" w) $(custom_widget_meta "$id" h)"
        [[ -n "$monitor" ]] && rules="monitor $monitor silent;$rules"
        hyprctl dispatch exec "[$rules] $command" >/dev/null 2>&1 &
    fi
}

stop_custom_widget() {
    local id="$1"
    local wm_class mode

    mode="$(custom_widget_meta "$id" mode 2>/dev/null || printf 'app')"
    wm_class="$(custom_widget_meta "$id" class 2>/dev/null)" || return 0
    if [[ "$mode" == "terminal" ]]; then
        stop_widget "$id" "$wm_class" 2>/dev/null || true
    fi
    hyprctl clients -j 2>/dev/null |
        jq -r --arg class "$wm_class" '.[] | select((.class // "") == $class or (.initialClass // "") == $class) | .address' |
        while read -r addr; do
            [[ -n "$addr" ]] && hyprctl dispatch closewindow "address:$addr" >/dev/null 2>&1 || true
        done
}
