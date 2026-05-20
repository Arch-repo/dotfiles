#!/usr/bin/env bash
# Layout persistence for anto426 desktop widgets (Hyprland floating windows).

widget_layout_file() {
    printf '%s' "${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets_layout.env"
}

widget_meta() {
    local name="$1"
    local field="$2"

    case "$name" in
        clock)
            case "$field" in
                class) printf 'anto426.widget.clock' ;;
                title) printf 'Clock Widget' ;;
                w) printf '300' ;;
                h) printf '140' ;;
                dx) printf '40' ;;
                dy) printf '72' ;;
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
            esac
            ;;
    esac
}

widget_order_default() {
    printf '%s' "${ANTO426_WIDGET_ORDER:-clock cava system}"
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
    local upper="${name^^}"
    local var="LAYOUT_${upper}_${key}"
    printf '%s' "${!var:-}"
}

layout_set_var() {
    local name="$1"
    local key="$2"
    local value="$3"
    local file tmp upper var
    upper="${name^^}"
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

    hyprctl --batch "dispatch resizewindowpixel exact $w $h,address:$addr; dispatch movewindowpixel exact $x $y,address:$addr" >/dev/null 2>&1
    hyprctl dispatch setfloating "address:$addr" >/dev/null 2>&1
    return 0
}

layout_default_stack() {
    local name x y w h gap order monitor_x monitor_y
    local -a stack

    gap="${ANTO426_WIDGET_GAP:-18}"
    read -r monitor_x monitor_y < <(
        hyprctl monitors -j 2>/dev/null |
            jq -r '.[] | select(.focused == true) | "\(.x) \(.y)"' |
            sed -n '1p'
    )
    [[ -n "${monitor_x:-}" ]] || monitor_x=0
    [[ -n "${monitor_y:-}" ]] || monitor_y=0

    x=$((monitor_x + 40))
    y=$((monitor_y + 72))

    while IFS= read -r name; do
        [[ -n "$name" ]] && stack+=("$name")
    done < <(printf '%s\n' $(widget_order_default))

    for name in "${stack[@]}"; do
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

        y=$((y + h + gap))
    done
}

apply_widget_layout() {
    local name attempts=0
    local order

    layout_load
    order="$(widget_order_default)"

    while ((attempts < 24)); do
        local ready=0 total=0
        for name in $order; do
            case "$name" in clock | cava | system) total=$((total + 1)) ;; esac
        done

        for name in $order; do
            case "$name" in
                clock | cava | system)
                    local x y w h
                    x="$(layout_var "$name" x)"
                    y="$(layout_var "$name" y)"
                    w="$(layout_var "$name" w)"
                    h="$(layout_var "$name" h)"

                    [[ -z "$x" || -z "$y" ]] && {
                        x="$(widget_meta "$name" dx)"
                        y="$(widget_meta "$name" dy)"
                    }
                    [[ -z "$w" ]] && w="$(widget_meta "$name" w)"
                    [[ -z "$h" ]] && h="$(widget_meta "$name" h)"

                    if apply_widget_geometry "$name" "$x" "$y" "$w" "$h"; then
                        ready=$((ready + 1))
                    fi
                    ;;
            esac
        done

        ((ready >= total)) && ((total > 0)) && return 0
        sleep 0.15
        attempts=$((attempts + 1))
    done

    return 1
}

save_widget_layout() {
    local name x y w h addr file tmp
    local order saved_order

    layout_ensure_file
    file="$(widget_layout_file)"
    order="$(widget_order_default)"

    for name in $order clock cava system; do
        case "$name" in
            clock | cava | system) ;;
            *) continue ;;
        esac

        addr="$(widget_client_address "$name")"
        [[ -n "$addr" ]] || continue

        read -r x y w h < <(
            hyprctl clients -j 2>/dev/null |
                jq -r --arg addr "$addr" '
                    .[] | select(.address == $addr) |
                    "\(.at[0]) \(.at[1]) \(.size[0]) \(.size[1])"
                ' |
                sed -n '1p'
        )
        [[ -n "$x" ]] || continue

        layout_set_var "$name" x "$x"
        layout_set_var "$name" y "$y"
        layout_set_var "$name" w "$w"
        layout_set_var "$name" h "$h"
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
