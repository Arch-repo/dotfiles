#!/usr/bin/env bash
# Layout persistence for anto426 desktop widgets (Hyprland floating windows).

widget_layout_file() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets_layout.env"
}

layout_var_prefix() {
    printf '%s' "$1" | tr '[:lower:]' '[:upper:]' | sed 's/[^A-Z0-9_]/_/g'
}

widget_is_builtin() {
    case "${1:-}" in
        clock | cava | system) return 0 ;;
        *) return 1 ;;
    esac
}

builtin_widget_ids() {
    printf '%s\n' clock cava system
}

enabled_builtin_widgets() {
    local enabled name

    enabled="${ANTO426_WIDGETS_ENABLED-clock cava system}"
    for name in $enabled; do
        widget_is_builtin "$name" && printf '%s\n' "$name"
    done
}

all_known_widget_ids() {
    local name

    builtin_widget_ids
    if [[ -f "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh" ]]; then
        # shellcheck disable=SC1091
        source "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh"
        for name in $(custom_widget_ids); do
            custom_widget_meta "$name" id >/dev/null 2>&1 && printf '%s\n' "$name"
        done
    fi
}

widget_meta() {
    local name="$1"
    local field="$2"

    if ! widget_is_builtin "$name"; then
        if [[ -f "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh" ]]; then
            # shellcheck disable=SC1091
            source "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh"
            custom_widget_meta "$name" "$field" && return 0
        fi
        return 1
    fi

    case "$name" in
        clock)
            case "$field" in
                class) printf 'anto426.widget.clock' ;;
                title) printf 'Clock Widget' ;;
                w) printf '300' ;;
                h) printf '140' ;;
                dx) printf '40' ;;
                dy) printf '72' ;;
                monitor) layout_var clock monitor ;;
            esac
            ;;
        cava)
            case "$field" in
                class) printf 'anto426.widget.cava' ;;
                title) printf 'Cava Widget' ;;
                w) printf '500' ;;
                h) printf '168' ;;
                dx) printf '40' ;;
                dy) printf '230' ;;
                monitor) layout_var cava monitor ;;
            esac
            ;;
        system)
            case "$field" in
                class) printf 'anto426.widget.system' ;;
                title) printf 'System Widget' ;;
                w) printf '380' ;;
                h) printf '210' ;;
                dx) printf '40' ;;
                dy) printf '418' ;;
                monitor) layout_var system monitor ;;
            esac
            ;;
    esac
}

widget_order_default() {
    local saved enabled custom name out

    out=""
    saved="${ANTO426_WIDGET_ORDER-}"
    enabled="$(enabled_builtin_widgets | tr '\n' ' ')"
    if [[ -f "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh" ]]; then
        # shellcheck disable=SC1091
        source "$(dirname "${BASH_SOURCE[0]}")/widgets_apps.sh"
        custom="$(custom_widget_ids | tr '\n' ' ')"
    fi

    for name in $saved $enabled ${custom:-}; do
        [[ -n "$name" ]] || continue
        case " $enabled ${custom:-} " in
            *" $name "*) ;;
            *) continue ;;
        esac
        widget_meta "$name" id >/dev/null 2>&1 ||
            widget_is_builtin "$name" ||
            continue
        case " $out " in
            *" $name "*) ;;
            *) out="${out:+$out }$name" ;;
        esac
    done

    printf '%s' "$out"
}

layout_ensure_file() {
    local file
    file="$(widget_layout_file)"
    [[ -f "$file" ]] && return 0

    mkdir -p "$(dirname "$file")"
    cat >"$file" <<'EOF'
# Posizioni widget (aggiornate con: widgets.sh save-layout)
# Riordina con: widgets.sh arrange
export ANTO426_WIDGET_ORDER="clock cava system"
EOF
}

layout_load() {
    layout_ensure_file
    # shellcheck disable=SC1090
    source "$(widget_layout_file)"
}

layout_var() {
    local name="$1"
    local key="$2"
    local upper
    local var
    upper="$(layout_var_prefix "$name")"
    var="LAYOUT_${upper}_${key}"
    printf '%s' "${!var:-}"
}

layout_set_var() {
    local name="$1"
    local key="$2"
    local value="$3"
    local file tmp upper var
    upper="$(layout_var_prefix "$name")"
    var="LAYOUT_${upper}_${key}"

    layout_ensure_file
    file="$(widget_layout_file)"
    tmp="$(mktemp)"
    awk -v key="$var" -v value="$value" '
        BEGIN { found = 0 }
        $0 ~ "^export " key "=" {
            print "export " key "=\"" value "\""
            found = 1
            next
        }
        { print }
        END {
            if (!found) print "export " key "=\"" value "\""
        }
    ' "$file" >"$tmp"
    mv "$tmp" "$file"
    printf -v "$var" '%s' "$value"
    export "$var"
}

focused_monitor_name() {
    hyprctl monitors -j 2>/dev/null |
        jq -r '.[] | select(.focused == true) | .name' 2>/dev/null |
        sed -n '1p'
}

monitor_geometry() {
    local monitor="$1"

    hyprctl monitors -j 2>/dev/null |
        jq -r --arg monitor "$monitor" '
            def row: "\(.x) \(.y) \(.width) \(.height) \(.name)";
            if $monitor == "" then
                (.[] | select(.focused == true) | row)
            else
                (.[] | select(.name == $monitor) | row)
            end
        ' 2>/dev/null |
        sed -n '1p'
}

monitor_name_by_id() {
    local monitor_id="$1"

    [[ "$monitor_id" =~ ^[0-9]+$ ]] || return 1
    hyprctl monitors -j 2>/dev/null |
        jq -r --argjson monitor_id "$monitor_id" '
            .[] | select(.id == $monitor_id) | .name
        ' 2>/dev/null |
        sed -n '1p'
}

monitor_name_for_point() {
    local x="$1"
    local y="$2"

    hyprctl monitors -j 2>/dev/null |
        jq -r --argjson x "${x:-0}" --argjson y "${y:-0}" '
            .[] |
            select($x >= .x and $x < (.x + .width) and $y >= .y and $y < (.y + .height)) |
            .name
        ' 2>/dev/null |
        sed -n '1p'
}

layout_monitor_for_widget() {
    local name="$1"
    local monitor

    monitor="$(layout_var "$name" monitor)"
    [[ -n "$monitor" ]] || monitor="$(widget_meta "$name" monitor 2>/dev/null || true)"
    [[ -n "$monitor" ]] || monitor="$(focused_monitor_name)"
    printf '%s' "$monitor"
}

widget_client_address() {
    local name="$1"
    local class title

    class="$(widget_meta "$name" class)"
    title="$(widget_meta "$name" title)"

    hyprctl clients -j 2>/dev/null |
        jq -r --arg class "$class" --arg title "$title" '
            .[] |
            select(
                (.class == $class) or
                (.initialClass == $class) or
                (.title == $title)
            ) |
            .address
        ' |
        sed -n '1p'
}

apply_widget_geometry() {
    local name="$1"
    local x="$2"
    local y="$3"
    local w="$4"
    local h="$5"
    local addr

    addr="$(widget_client_address "$name")"
    [[ -n "$addr" ]] || return 1

    hyprctl --batch "dispatch setfloating address:$addr; dispatch resizewindowpixel exact $w $h,address:$addr; dispatch movewindowpixel exact $x $y,address:$addr" >/dev/null 2>&1
    return 0
}

layout_default_stack() {
    local name x y w h gap order monitor monitor_x monitor_y monitor_width monitor_height actual_monitor
    local -a stack
    declare -A stack_x
    declare -A stack_y

    gap="${ANTO426_WIDGET_GAP:-18}"

    while IFS= read -r name; do
        [[ -n "$name" ]] && stack+=("$name")
    done < <(printf '%s\n' $(widget_order_default))

    for name in "${stack[@]}"; do
        monitor="$(layout_monitor_for_widget "$name")"
        read -r monitor_x monitor_y monitor_width monitor_height actual_monitor < <(monitor_geometry "$monitor")
        if [[ -z "${actual_monitor:-}" ]]; then
            read -r monitor_x monitor_y monitor_width monitor_height actual_monitor < <(monitor_geometry "")
        fi
        [[ -n "${actual_monitor:-}" ]] || actual_monitor="default"
        [[ -n "${monitor_x:-}" ]] || monitor_x=0
        [[ -n "${monitor_y:-}" ]] || monitor_y=0

        if [[ -z "${stack_y[$actual_monitor]+set}" ]]; then
            stack_x["$actual_monitor"]=$((monitor_x + 40))
            stack_y["$actual_monitor"]=$((monitor_y + 72))
        fi

        x="${stack_x[$actual_monitor]}"
        y="${stack_y[$actual_monitor]}"
        w="$(layout_var "$name" w)"
        h="$(layout_var "$name" h)"
        [[ -n "$w" && -n "$h" ]] || {
            w="$(widget_meta "$name" w)"
            h="$(widget_meta "$name" h)"
        }

        layout_set_var "$name" x "$x"
        layout_set_var "$name" y "$y"
        layout_set_var "$name" w "$w"
        layout_set_var "$name" h "$h"
        layout_set_var "$name" monitor "$actual_monitor"

        stack_y["$actual_monitor"]=$((y + h + gap))
    done
}

layout_place_widget_default() {
    local name="$1"
    local monitor="${2:-}"
    local x y w h gap other other_monitor other_y other_h next_y
    local monitor_x monitor_y monitor_width monitor_height actual_monitor

    layout_load
    [[ -n "$monitor" ]] || monitor="$(layout_monitor_for_widget "$name")"
    read -r monitor_x monitor_y monitor_width monitor_height actual_monitor < <(monitor_geometry "$monitor")
    if [[ -z "${actual_monitor:-}" ]]; then
        read -r monitor_x monitor_y monitor_width monitor_height actual_monitor < <(monitor_geometry "")
    fi
    [[ -n "${actual_monitor:-}" ]] || actual_monitor="default"
    [[ -n "${monitor_x:-}" ]] || monitor_x=0
    [[ -n "${monitor_y:-}" ]] || monitor_y=0

    gap="${ANTO426_WIDGET_GAP:-18}"
    x=$((monitor_x + 40))
    y=$((monitor_y + 72))

    for other in $(widget_order_default); do
        [[ -n "$other" && "$other" != "$name" ]] || continue
        other_monitor="$(layout_monitor_for_widget "$other")"
        [[ "$other_monitor" == "$actual_monitor" ]] || continue
        other_y="$(layout_var "$other" y)"
        other_h="$(layout_var "$other" h)"
        [[ "$other_y" =~ ^-?[0-9]+$ && "$other_h" =~ ^[0-9]+$ ]] || continue
        next_y=$((other_y + other_h + gap))
        ((next_y > y)) && y="$next_y"
    done

    w="$(layout_var "$name" w)"
    h="$(layout_var "$name" h)"
    [[ -n "$w" ]] || w="$(widget_meta "$name" w)"
    [[ -n "$h" ]] || h="$(widget_meta "$name" h)"

    layout_set_var "$name" x "$x"
    layout_set_var "$name" y "$y"
    layout_set_var "$name" w "$w"
    layout_set_var "$name" h "$h"
    layout_set_var "$name" monitor "$actual_monitor"
}

apply_widget_layout() {
    local name attempts=0
    local order

    layout_load
    order="$(widget_order_default)"

    while ((attempts < 24)); do
        local ready=0 total=0
        for name in $order; do
            [[ -n "$name" ]] && total=$((total + 1))
        done
        ((total == 0)) && return 0

        for name in $order; do
            [[ -n "$name" ]] || continue
            local x y w h
            x="$(layout_var "$name" x)"
            y="$(layout_var "$name" y)"
            w="$(layout_var "$name" w)"
            h="$(layout_var "$name" h)"

            [[ -z "$x" || -z "$y" ]] && {
                x="$(widget_meta "$name" dx 2>/dev/null || widget_meta "$name" w)"
                y="$(widget_meta "$name" dy 2>/dev/null || printf '72')"
            }
            [[ -z "$w" ]] && w="$(widget_meta "$name" w)"
            [[ -z "$h" ]] && h="$(widget_meta "$name" h)"

            if apply_widget_geometry "$name" "$x" "$y" "$w" "$h"; then
                ready=$((ready + 1))
            fi
        done

        ((ready >= total)) && ((total > 0)) && return 0
        sleep 0.15
        attempts=$((attempts + 1))
    done

    return 1
}

save_widget_layout() {
    local name x y w h addr file tmp monitor_id monitor
    local order saved_order

    saved_order=""
    layout_ensure_file
    file="$(widget_layout_file)"
    order="$(widget_order_default)"

    for name in $order; do
        [[ -n "$name" ]] || continue
        addr="$(widget_client_address "$name")"
        [[ -n "$addr" ]] || continue

        read -r x y w h monitor_id < <(
            hyprctl clients -j 2>/dev/null |
                jq -r --arg addr "$addr" '
                    .[] | select(.address == $addr) |
                    "\(.at[0]) \(.at[1]) \(.size[0]) \(.size[1]) \(.monitor // "")"
                ' |
                sed -n '1p'
        )
        [[ -n "$x" ]] || continue

        layout_set_var "$name" x "$x"
        layout_set_var "$name" y "$y"
        layout_set_var "$name" w "$w"
        layout_set_var "$name" h "$h"
        monitor="$(monitor_name_by_id "$monitor_id")"
        [[ -n "$monitor" ]] || monitor="$(monitor_name_for_point "$x" "$y")"
        [[ -n "$monitor" ]] && layout_set_var "$name" monitor "$monitor"
        saved_order="${saved_order:+$saved_order }$name"
    done

    if [[ -n "$saved_order" ]]; then
        tmp="$(mktemp)"
        awk -v order="$saved_order" '
            /^export ANTO426_WIDGET_ORDER=/ {
                print "export ANTO426_WIDGET_ORDER=\"" order "\""
                next
            }
            { print }
        ' "$file" >"$tmp"
        mv "$tmp" "$file"
    fi
}

reset_widget_layout() {
    rm -f "$(widget_layout_file)"
    layout_default_stack
    apply_widget_layout
}
