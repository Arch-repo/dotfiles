#!/usr/bin/env bash
# Layout persistence for anto426 desktop widgets (Hyprland floating windows).

pid_file() {
    local runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
    printf '%s/%s.pid' "$runtime_dir" "$1"
}

is_running() {
    local name="$1"
    local addr
    addr="$(widget_client_address "$name")"
    [[ -n "$addr" ]] && return 0

    local pfile
    pfile="$(pid_file "$name")"
    [[ -r "$pfile" ]] || return 1
    local pid
    pid="$(cat "$pfile" 2>/dev/null || true)"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null
}

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

layout_remove_widget_vars() {
    local name="$1"
    local file tmp prefix

    layout_ensure_file
    file="$(widget_layout_file)"
    tmp="$(mktemp)"
    prefix="$(layout_var_prefix "$name")"

    awk -v prefix="$prefix" '
        $0 ~ "^export LAYOUT_" prefix "_(x|y|w|h|monitor)=" { next }
        { print }
    ' "$file" >"$tmp" && mv "$tmp" "$file"

    unset \
        "LAYOUT_${prefix}_x" \
        "LAYOUT_${prefix}_y" \
        "LAYOUT_${prefix}_w" \
        "LAYOUT_${prefix}_h" \
        "LAYOUT_${prefix}_monitor"
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
    local clients_json_input="${2:-}"
    local runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
    local pid_file="$runtime_dir/$name.pid"
    local pid class title
    local clients_json

    if [[ -n "$clients_json_input" ]]; then
        clients_json="$clients_json_input"
    else
        clients_json="$(hyprctl clients -j 2>/dev/null || echo "[]")"
    fi

    if [[ -r "$pid_file" ]]; then
        pid="$(cat "$pid_file" 2>/dev/null || true)"
        if [[ "$pid" =~ ^[0-9]+$ ]]; then
            local addr
            addr="$(echo "$clients_json" | jq -r --argjson pid "$pid" '.[] | select(.pid == $pid) | .address' | sed -n '1p')"
            if [[ -n "$addr" ]]; then
                printf '%s' "$addr"
                return 0
            fi
        fi
    fi

    class="$(widget_meta "$name" class 2>/dev/null || true)"
    title="$(widget_meta "$name" title 2>/dev/null || true)"
    [[ -n "$class" ]] || return 1

    echo "$clients_json" |
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
    local clients_json="${6:-}"
    local addr

    addr="$(widget_client_address "$name" "$clients_json")"
    [[ -n "$addr" ]] || return 1

    hyprctl --batch "dispatch setfloating address:$addr; dispatch resizewindowpixel exact $w $h,address:$addr; dispatch movewindowpixel exact $x $y,address:$addr" >/dev/null 2>&1
    return 0
}

layout_default_stack() {
    local name x y w h gap order monitor monitor_x monitor_y monitor_width monitor_height actual_monitor
    local -a stack
    declare -A stack_x
    declare -A stack_y
    declare -A stack_column_width

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
        [[ -n "${monitor_height:-}" ]] || monitor_height=1080
        [[ -n "${monitor_width:-}" ]] || monitor_width=1920

        if [[ -z "${stack_y[$actual_monitor]+set}" ]]; then
            stack_x["$actual_monitor"]=$((monitor_x + 40))
            stack_y["$actual_monitor"]=$((monitor_y + 72))
            stack_column_width["$actual_monitor"]=0
        fi

        x="${stack_x[$actual_monitor]}"
        y="${stack_y[$actual_monitor]}"
        w="$(layout_var "$name" w)"
        h="$(layout_var "$name" h)"
        [[ -n "$w" && -n "$h" ]] || {
            w="$(widget_meta "$name" w)"
            h="$(widget_meta "$name" h)"
        }

        # Wrap if it exceeds monitor height
        local bottom_edge=$((monitor_y + monitor_height - 60))
        if (( y + h > bottom_edge )) && (( y > monitor_y + 72 )); then
            local next_col_x=$(( x + stack_column_width["$actual_monitor"] + gap ))
            stack_x["$actual_monitor"]=$next_col_x
            stack_y["$actual_monitor"]=$((monitor_y + 72))
            x=$next_col_x
            y=$((monitor_y + 72))
            stack_column_width["$actual_monitor"]=0
        fi

        local col_w="${stack_column_width["$actual_monitor"]}"
        if (( w > col_w )); then
            stack_column_width["$actual_monitor"]=$w
        fi

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
    local x y w h gap other other_monitor
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
    [[ -n "${monitor_height:-}" ]] || monitor_height=1080
    [[ -n "${monitor_width:-}" ]] || monitor_width=1920

    w="$(layout_var "$name" w)"
    h="$(layout_var "$name" h)"
    [[ -n "$w" ]] || w="$(widget_meta "$name" w)"
    [[ -n "$h" ]] || h="$(widget_meta "$name" h)"

    gap="${ANTO426_WIDGET_GAP:-18}"
    x=$((monitor_x + 40))
    y=$((monitor_y + 72))
    local bottom_edge=$((monitor_y + monitor_height - 60))

    local collided=1
    local safety_counter=0
    while (( collided == 1 )) && (( safety_counter < 100 )); do
        collided=0
        safety_counter=$((safety_counter + 1))
        local max_w_in_col=$w
        local intersect_widget=""
        local intersect_y=0
        local intersect_h=0

        for other in $(widget_order_default); do
            [[ -n "$other" && "$other" != "$name" ]] || continue
            other_monitor="$(layout_monitor_for_widget "$other")"
            [[ "$other_monitor" == "$actual_monitor" ]] || continue

            local ox oy ow oh
            ox="$(layout_var "$other" x)"
            oy="$(layout_var "$other" y)"
            ow="$(layout_var "$other" w)"
            oh="$(layout_var "$other" h)"
            [[ "$ox" =~ ^-?[0-9]+$ && "$oy" =~ ^-?[0-9]+$ && "$ow" =~ ^[0-9]+$ && "$oh" =~ ^[0-9]+$ ]] || continue

            # Check rectangle collision with gap padding
            if (( x < ox + ow + gap )) && (( x + w + gap > ox )) && (( y < oy + oh + gap )) && (( y + h + gap > oy )); then
                collided=1
                if [[ -z "$intersect_widget" ]] || (( oy > intersect_y )); then
                    intersect_widget="$other"
                    intersect_y="$oy"
                    intersect_h="$oh"
                fi
            fi

            # If in the same vertical column range, track column width
            if (( ox + ow + gap > x )) && (( ox < x + w + gap )); then
                if (( ow > max_w_in_col )); then
                    max_w_in_col=$ow
                fi
            fi
        done

        if (( collided == 1 )); then
            y=$(( intersect_y + intersect_h + gap ))

            if (( y + h > bottom_edge )); then
                local rightmost_x=$(( x + max_w_in_col ))
                for other in $(widget_order_default); do
                    [[ -n "$other" && "$other" != "$name" ]] || continue
                    other_monitor="$(layout_monitor_for_widget "$other")"
                    [[ "$other_monitor" == "$actual_monitor" ]] || continue
                    local ox ow
                    ox="$(layout_var "$other" x)"
                    ow="$(layout_var "$other" w)"
                    [[ "$ox" =~ ^-?[0-9]+$ && "$ow" =~ ^[0-9]+$ ]] || continue
                    if (( ox + ow + gap > x )) && (( ox < x + w + gap )); then
                        if (( ox + ow > rightmost_x )); then
                            rightmost_x=$(( ox + ow ))
                        fi
                    fi
                done
                x=$(( rightmost_x + gap ))
                y=$(( monitor_y + 72 ))
            fi
        fi
    done

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
        local clients_json
        clients_json="$(hyprctl clients -j 2>/dev/null || echo "[]")"

        for name in $order; do
            [[ -n "$name" ]] || continue
            is_running "$name" || continue
            total=$((total + 1))
        done
        ((total == 0)) && return 0

        for name in $order; do
            [[ -n "$name" ]] || continue
            is_running "$name" || continue
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

            if apply_widget_geometry "$name" "$x" "$y" "$w" "$h" "$clients_json"; then
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
