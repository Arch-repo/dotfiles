#!/usr/bin/env bash

custom_widgets_file() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets_custom.env"
}

ensure_custom_widgets_file() {
    local file
    file="$(custom_widgets_file)"
    [[ -f "$file" ]] || cat >"$file" <<'EOF'
# Custom app widgets: id|Name|StartupWMClass|exec command
# Example:
# notes|Note|obsidian|obsidian
export ANTO426_CUSTOM_WIDGETS=""
EOF
}

custom_widget_ids() {
    ensure_custom_widgets_file
    # shellcheck disable=SC1090
    source "$(custom_widgets_file)"
    printf '%s\n' ${ANTO426_CUSTOM_WIDGETS:-}
}

custom_widget_meta() {
    local id="$1"
    local field="$2"
    local file line name class cmd

    file="$(custom_widgets_file)"
    line="$(
        awk -F'|' -v id="$id" '$1 == id { print; exit }' "$file" 2>/dev/null
    )"
    [[ -n "$line" ]] || return 1

    name="$(printf '%s' "$line" | awk -F'|' '{print $2}')"
    class="$(printf '%s' "$line" | awk -F'|' '{print $3}')"
    cmd="$(printf '%s' "$line" | awk -F'|' '{print $4}')"

    case "$field" in
        id) printf '%s' "$id" ;;
        name) printf '%s' "$name" ;;
        class) printf '%s' "$class" ;;
        hypr_class) printf 'anto426.widget.app.%s' "$id" ;;
        title) printf '%s' "$name" ;;
        command) printf '%s' "$cmd" ;;
        w) printf '520' ;;
        h) printf '360' ;;
    esac
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

    exec_line="${exec_line%%[[:space:]]*--*}"
    exec_line="${exec_line//\%/%%}"
    exec_line="$(printf '%b' "$exec_line")"
    printf '%s' "$exec_line"
}

pick_app_for_widget() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice desktop id name startup class exec_line file tmp

    choice="$(
        find \
            "$HOME/.local/share/applications" \
            /usr/share/applications \
            -maxdepth 1 -type f -name '*.desktop' 2>/dev/null |
            while read -r desktop; do
                [[ "$(awk -F= '/^Type=/ {print tolower($2); exit}' "$desktop")" == "application" ]] || continue
                [[ "$(awk -F= '/^NoDisplay=/ {print tolower($2); exit}' "$desktop")" == "true" ]] && continue
                [[ "$(awk -F= '/^Hidden=/ {print tolower($2); exit}' "$desktop")" == "true" ]] && continue
                name="$(awk -F= '/^Name=/{print substr($0,index($0,$2)); exit}' "$desktop")"
                icon="$(awk -F= 'tolower($1)=="icon"{print $2; exit}' "$desktop")"
                [[ -n "$name" ]] || continue
                printf '%s\0icon\x1f%s\n' "$name" "${icon:-application-x-executable}"
            done |
            rofi -dmenu -i -matching fuzzy -show-icons -p "Aggiungi widget app" -theme "$theme"
    )"

    [[ -n "$choice" ]] || return 1

    desktop="$(
        find \
            "$HOME/.local/share/applications" \
            /usr/share/applications \
            -maxdepth 1 -type f -name '*.desktop' 2>/dev/null |
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
    id="$(printf '%s' "$startup" | tr '[:upper:]' '[:lower:]' | tr -c 'a-z0-9._-' '_')"

    ensure_custom_widgets_file
    file="$(custom_widgets_file)"
    tmp="$(mktemp)"
    awk -F'|' -v id="$id" '$1 != id { print }' "$file" >"$tmp"
    printf '%s|%s|%s|%s\n' "$id" "$name" "$startup" "$exec_line" >>"$tmp"

    if grep -q '^export ANTO426_CUSTOM_WIDGETS=' "$tmp"; then
        awk -v id="$id" '
            /^export ANTO426_CUSTOM_WIDGETS=/ {
                split($0, parts, "=")
                value = parts[2]
                gsub(/"/, "", value)
                if (value == "") ids = id
                else if (index(" " value " ", " " id " ") == 0) ids = value " " id
                else ids = value
                print "export ANTO426_CUSTOM_WIDGETS=\"" ids "\""
                next
            }
            { print }
        ' "$tmp" >"${tmp}.2" && mv "${tmp}.2" "$tmp"
    else
        printf 'export ANTO426_CUSTOM_WIDGETS="%s"\n' "$id" >>"$tmp"
    fi
    mv "$tmp" "$file"
    write_custom_widget_hypr_rules
    printf '%s' "$id"
}

write_custom_widget_hypr_rules() {
    local file="$HOME/.config/hypr/conf/widget-apps.generated.conf"
    local id wm_class

    {
        printf '# Generated by widgets_apps.sh — regole float per widget app\n'
        for id in $(custom_widget_ids); do
            [[ -n "$id" ]] || continue
            wm_class="$(custom_widget_meta "$id" class 2>/dev/null)" || continue
            printf 'windowrule = float on, match:class ^%s$\n' "$wm_class"
            printf 'windowrule = border_size 1, match:class ^%s$\n' "$wm_class"
            printf 'windowrule = no_shadow on, match:class ^%s$\n' "$wm_class"
            printf 'windowrule = no_anim on, match:class ^%s$\n' "$wm_class"
            printf 'windowrule = rounding 14, match:class ^%s$\n' "$wm_class"
            printf 'windowrule = opacity 0.94 0.88, match:class ^%s$\n' "$wm_class"
        done
    } >"$file"

    hyprctl reload >/dev/null 2>&1 || true
}

launch_custom_widget() {
    local id="$1"

    custom_widget_meta "$id" id >/dev/null || return 1
    write_custom_widget_hypr_rules
    hyprctl dispatch exec "[float;size $(custom_widget_meta "$id" w) $(custom_widget_meta "$id" h)] $(custom_widget_meta "$id" command)" >/dev/null 2>&1 &
}

stop_custom_widget() {
    local id="$1"
    local wm_class
    wm_class="$(custom_widget_meta "$id" class 2>/dev/null)" || return 0
    hyprctl clients -j 2>/dev/null |
        jq -r --arg class "$wm_class" '.[] | select((.class // "") == $class or (.initialClass // "") == $class) | .address' |
        while read -r addr; do
            [[ -n "$addr" ]] && hyprctl dispatch closewindow "address:$addr" >/dev/null 2>&1 || true
        done
}
