#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=widgets_layout.sh
source "$SCRIPT_DIR/widgets_layout.sh"
# shellcheck source=widgets_apps.sh
source "$SCRIPT_DIR/widgets_apps.sh"

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
lock_daemon_pid="$runtime_dir/lock-daemon.pid"
delete_watcher_pid="$runtime_dir/delete-watcher.pid"
managed_stop_marker="$runtime_dir/managed-stop"
config_file="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets.env"
cava_config="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/cava_widget.conf"
colors_file="${XDG_CONFIG_HOME:-$HOME/.config}/colors/colors.sh"
widget_lock_rules_file="${XDG_CONFIG_HOME:-$HOME/.config}/hypr/conf/widget-lock.generated.conf"
mkdir -p "$runtime_dir"

ensure_config() {
    if [[ ! -f "$config_file" ]]; then
        cat >"$config_file" <<'EOF'
# Set to 0 to prevent widgets from starting with Hyprland.
export ANTO426_WIDGETS_AUTOSTART=1

# Available widgets: clock cava system
export ANTO426_WIDGETS_ENABLED="clock cava system"

# Ordine verticale predefinito (riordina con: widgets.sh arrange)
export ANTO426_WIDGET_ORDER="clock cava system"

# Spazio tra widget quando si resetta il layout
export ANTO426_WIDGET_GAP=18

# Terminale usato per i widget (ghostty consigliato)
export ANTO426_WIDGETS_BACKEND="terminal"

# 1 = widget bloccati: non prendono focus e non si spostano per sbaglio.
export ANTO426_WIDGETS_LOCKED=0
EOF
    fi

    # shellcheck disable=SC1090
    source "$config_file"
}

if [[ -f "$config_file" ]]; then
    # shellcheck disable=SC1090
    source "$config_file"
fi

notify() {
    notify-send "Widget" "$*" 2>/dev/null || true
}

terminal_cmd() {
    if command -v ghostty >/dev/null 2>&1; then
        printf 'ghostty'
        return 0
    fi
    if command -v kitty >/dev/null 2>&1; then
        printf 'kitty'
        return 0
    fi
    if command -v foot >/dev/null 2>&1; then
        printf 'foot'
        return 0
    fi
    if command -v alacritty >/dev/null 2>&1; then
        printf 'alacritty'
        return 0
    fi
    return 1
}

launch_terminal() {
    local terminal="$1"
    local class="$2"
    local title="$3"
    local command="$4"

    case "$terminal" in
        ghostty)
            "$terminal" \
                --class="$class" \
                --title="$title" \
                --font-size=11 \
                --font-family="JetBrainsMono Nerd Font" \
                --window-padding-x=16 \
                --window-padding-y=12 \
                --background-opacity=0.74 \
                --window-decoration=false \
                --confirm-close-surface=false \
                -e bash -lc "$command" >/dev/null 2>&1 &
            ;;
        kitty)
            "$terminal" \
                --class "$class" \
                --title "$title" \
                --override "font_size=11" \
                --override "window_padding_width=14" \
                --override "background_opacity=0.74" \
                --override "hide_window_decorations=yes" \
                bash -lc "$command" >/dev/null 2>&1 &
            ;;
        foot)
            "$terminal" --app-id "$class" --title "$title" bash -lc "$command" >/dev/null 2>&1 &
            ;;
        alacritty)
            "$terminal" --class "$class","$class" --title "$title" -e bash -lc "$command" >/dev/null 2>&1 &
            ;;
        *)
            return 1
            ;;
    esac
}

pid_file() {
    printf '%s/%s.pid' "$runtime_dir" "$1"
}

is_running() {
    local name="$1"
    local pid_file pid

    pid_file="$(pid_file "$name")"
    [[ -r "$pid_file" ]] || return 1
    pid="$(cat "$pid_file" 2>/dev/null || true)"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null
}

launch_widget() {
    local name="$1"
    local class="$2"
    local title="$3"
    local command="$4"
    local terminal pid

    is_running "$name" && return 0

    terminal="$(terminal_cmd)" || {
        notify "Terminale compatibile non trovato"
        return 1
    }

    if ! launch_terminal "$terminal" "$class" "$title" "$command"; then
        notify "Avvio widget fallito: $name"
        return 1
    fi

    pid=$!
    printf '%s\n' "$pid" >"$(pid_file "$name")"
}

stop_widget() {
    local name="$1"
    local class="$2"
    local pid_file pid

    pid_file="$(pid_file "$name")"
    pid="$(cat "$pid_file" 2>/dev/null || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        kill "$pid" 2>/dev/null || true
    fi
    pkill -f -- "$class" 2>/dev/null || true
    rm -f "$pid_file"
}

widget_shell_prefix() {
    local colors_source=""
    [[ -f "$colors_file" ]] && colors_source="source \"$colors_file\"; "
    printf '%s' "$colors_source"
}

clock_command="$(widget_shell_prefix)"
clock_command+='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
accent="${ANTO426_ACCENT:-#8cb8e4}"
fg="${ANTO426_FOREGROUND:-#f6f7fb}"
muted="${ANTO426_MUTED:-#b9c4d2}"
while true; do
    clear
    printf "\n"
    printf "  \033[1m%s\033[0m\n" "$(date +%H:%M)"
    printf "  %s\n" "$(date "+%A %d %B")"
    printf "  \033[2m%s\033[0m\n" "$(date "+%Y")"
    sleep 1
done
'

cava_command="$(widget_shell_prefix)"
cava_command+='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
if command -v cava >/dev/null 2>&1; then
    exec cava -p "'"$cava_config"'"
else
    while true; do
        clear
        printf "\n  󰎈  Audio visualizer\n\n"
        printf "  Installa cava per il visualizzatore\n"
        sleep 5
    done
fi
'

update_cava_theme_colors() {
    [[ -f "$cava_config" && -f "$colors_file" ]] || return 0
    # shellcheck disable=SC1090
    source "$colors_file"
    sed -i \
        -e "s|^gradient_1 = .*|gradient_1 = '${ANTO426_ACCENT:-#9579b6}'|" \
        -e "s|^gradient_2 = .*|gradient_2 = '${ANTO426_FOREGROUND:-#f6f7fb}'|" \
        "$cava_config"
}

layout_needs_defaults() {
    local name
    for name in clock cava system; do
        [[ -z "$(layout_var "$name" x)" || -z "$(layout_var "$name" y)" ]] && return 0
    done
    return 1
}

system_command="$(widget_shell_prefix)"
system_command+='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
bar() {
    value="${1:-0}"
    filled=$((value / 10))
    out=""
    i=0
    while [ "$i" -lt 10 ]; do
        if [ "$i" -lt "$filled" ]; then out="${out}█"; else out="${out}░"; fi
        i=$((i + 1))
    done
    printf "%s" "$out"
}
pct() {
    wpctl get-volume "$1" 2>/dev/null | awk "/^Volume:/ { v=int((\$2*100)+0.5); if(v<0)v=0; if(v>100)v=100; print v; exit }"
}
while true; do
    clear
    battery="n/d"
    for b in /sys/class/power_supply/BAT*; do
        [ -r "$b/capacity" ] && battery="$(cat "$b/capacity")%" && break
    done
    mem="$(free -h 2>/dev/null | awk "/^Mem:/ {print \$3 \" / \" \$2}")"
    load="$(cut -d" " -f1-3 /proc/loadavg 2>/dev/null)"
    vol="$(pct @DEFAULT_AUDIO_SINK@)"
    mic="$(pct @DEFAULT_AUDIO_SOURCE@)"
    brightness="$(brightnessctl -m 2>/dev/null | awk -F, "{gsub(/%/,\"\",\$4); print int(\$4); exit}")"
    temp="$(sensors 2>/dev/null | awk "/Package id 0|Tctl|Tdie/ {print \$3; exit}")"
    printf "\n  󰍛  Sistema\n\n"
    printf "  󰁹  Batteria   %4s  %s\n" "$battery" "$(bar "${battery%%%}")"
    printf "  󰍛  RAM        %s\n" "${mem:-n/d}"
    printf "  󰻠  CPU        %s\n" "${load:-n/d}"
    [[ -n "$temp" ]] && printf "  󰈸  Temp       %s\n" "$temp"
    printf "  󰕾  Volume     %3s%%  %s\n" "${vol:-?}" "$(bar "${vol:-0}")"
    printf "  󰍬  Mic        %3s%%  %s\n" "${mic:-?}" "$(bar "${mic:-0}")"
    [[ -n "$brightness" ]] && printf "  󰃠  Luce       %3s%%  %s\n" "$brightness" "$(bar "$brightness")"
    sleep 2
done
'

start_widgets() {
    local widget
    local quiet="${1:-false}"

    ensure_config
    layout_load
    update_cava_theme_colors
    if widgets_locked; then
        write_widget_lock_hypr_rules lock
    else
        write_widget_lock_hypr_rules unlock
    fi
    if layout_needs_defaults; then
        layout_default_stack
    fi

    for widget in ${ANTO426_WIDGETS_ENABLED:-clock cava system}; do
        case "$widget" in
            clock)
                launch_widget clock "$(widget_meta clock class)" "$(widget_meta clock title)" "$clock_command"
                ;;
            cava)
                launch_widget cava "$(widget_meta cava class)" "$(widget_meta cava title)" "$cava_command"
                ;;
            system)
                launch_widget system "$(widget_meta system class)" "$(widget_meta system title)" "$system_command"
                ;;
        esac
    done

    for widget in $(custom_widget_ids); do
        [[ -n "$widget" ]] && launch_custom_widget "$widget"
    done

    (
        if ! apply_widget_layout_locked; then
            layout_default_stack
            apply_widget_layout_locked
        fi
    ) &

    rm -f "$managed_stop_marker"
    if widgets_locked; then
        start_widgets_lock_daemon
    else
        start_widget_delete_watcher
    fi

    [[ "$quiet" == "quiet" ]] || notify "Widget avviati — trascina con Super + tasto sinistro"
}

stop_widgets() {
    local quiet="${1:-false}"

    touch "$managed_stop_marker"
    stop_widget_delete_watcher
    stop_widgets_lock_daemon
    save_widget_layout 2>/dev/null || true

    stop_widget clock "$(widget_meta clock class)"
    stop_widget cava "$(widget_meta cava class)"
    stop_widget system "$(widget_meta system class)"
    for widget in $(custom_widget_ids); do
        [[ -n "$widget" ]] && stop_custom_widget "$widget"
    done
    rm -f "$managed_stop_marker"
    [[ "$quiet" == "quiet" ]] || notify "Widget chiusi — layout salvato"
}

any_running() {
    local widget

    for widget in $(widget_order_default); do
        [[ -n "$widget" ]] || continue
        widget_client_address "$widget" | grep -q . && return 0
        is_running "$widget" && return 0
    done
    return 1
}

set_config_value() {
    local key="$1"
    local value="$2"
    local file tmp

    ensure_config
    file="$config_file"
    tmp="$(mktemp)"
    awk -v key="$key" -v value="$value" '
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
    ' "$file" >"$tmp" && mv "$tmp" "$file"
    # shellcheck disable=SC1090
    source "$file"
}

widgets_locked() {
    [[ "${ANTO426_WIDGETS_LOCKED:-0}" == "1" ]]
}

widget_addresses() {
    local name addr

    for name in $(widget_order_default); do
        [[ -n "$name" ]] || continue
        addr="$(widget_client_address "$name")"
        [[ -n "$addr" ]] && printf '%s\n' "$addr"
    done
}

widget_class_regex() {
    printf '^%s$' "$(printf '%s' "$1" | sed -e 's/[.[\*^$()+?{}|\\]/\\&/g')"
}

write_widget_lock_hypr_rules() {
    local state="${1:-current}"
    local reload="${2:-reload}"
    local locked class regex name x y w h file_dir

    ensure_config
    layout_load
    case "$state" in
        lock | locked | 1 | true) locked=1 ;;
        unlock | unlocked | 0 | false) locked=0 ;;
        *) widgets_locked && locked=1 || locked=0 ;;
    esac

    file_dir="$(dirname "$widget_lock_rules_file")"
    mkdir -p "$file_dir"

    {
        printf '# Generated by widgets.sh - lock state for desktop widgets\n'
        if [[ "$locked" == "1" ]]; then
            for name in $(widget_order_default); do
                [[ -n "$name" ]] || continue
                class="$(widget_meta "$name" class 2>/dev/null)" || continue
                regex="$(widget_class_regex "$class")"
                x="$(layout_var "$name" x)"
                y="$(layout_var "$name" y)"
                w="$(layout_var "$name" w)"
                h="$(layout_var "$name" h)"
                [[ -n "$x" && -n "$y" ]] || {
                    x="$(widget_meta "$name" dx 2>/dev/null || printf '40')"
                    y="$(widget_meta "$name" dy 2>/dev/null || printf '72')"
                }
                [[ -n "$w" ]] || w="$(widget_meta "$name" w 2>/dev/null || printf '520')"
                [[ -n "$h" ]] || h="$(widget_meta "$name" h 2>/dev/null || printf '320')"

                printf 'windowrule = match:class %s, float on\n' "$regex"
                printf 'windowrule = match:class %s, size %s %s\n' "$regex" "$w" "$h"
                printf 'windowrule = match:class %s, move %s %s\n' "$regex" "$x" "$y"
                printf 'windowrule = match:class %s, no_focus on\n' "$regex"
                printf 'windowrule = match:class %s, no_follow_mouse on\n' "$regex"
                printf 'windowrule = match:class %s, no_initial_focus on\n' "$regex"
                printf 'windowrule = match:class %s, focus_on_activate off\n' "$regex"
                printf 'windowrule = match:class %s, suppress_event activate activatefocus maximize fullscreen\n' "$regex"
                printf 'windowrule = match:class %s, border_size 0\n' "$regex"
                printf 'windowrule = match:class %s, dim_around off\n' "$regex"
            done
        fi
    } >"$widget_lock_rules_file"

    [[ "$reload" == "no-reload" ]] || hyprctl reload >/dev/null 2>&1 || true
}

bury_widget_windows() {
    local addr

    for addr in $(widget_addresses); do
        hyprctl dispatch alterzorder "bottom,address:$addr" >/dev/null 2>&1 || true
    done
}

sync_locked_widgets() {
    ensure_config
    widgets_locked || return 0
    apply_widget_layout >/dev/null 2>&1 || true
    apply_widgets_lock_state
    bury_widget_windows
}

widgets_lock_daemon_running() {
    local pid

    [[ -r "$lock_daemon_pid" ]] || return 1
    pid="$(cat "$lock_daemon_pid" 2>/dev/null || true)"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null
}

widget_delete_watcher_running() {
    local pid

    [[ -r "$delete_watcher_pid" ]] || return 1
    pid="$(cat "$delete_watcher_pid" 2>/dev/null || true)"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null
}

start_widgets_lock_daemon() {
    widgets_locked || return 0
    widgets_lock_daemon_running && return 0

    "$0" lock-daemon >/dev/null 2>&1 &
    printf '%s\n' "$!" >"$lock_daemon_pid"
}

start_widget_delete_watcher() {
    widgets_locked && return 0
    widget_delete_watcher_running && return 0

    "$0" delete-watcher >/dev/null 2>&1 &
    printf '%s\n' "$!" >"$delete_watcher_pid"
}

stop_widgets_lock_daemon() {
    local pid

    pid="$(cat "$lock_daemon_pid" 2>/dev/null || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        kill "$pid" 2>/dev/null || true
    fi
    rm -f "$lock_daemon_pid"
}

stop_widget_delete_watcher() {
    local pid

    pid="$(cat "$delete_watcher_pid" 2>/dev/null || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        kill "$pid" 2>/dev/null || true
    fi
    rm -f "$delete_watcher_pid"
}

run_widgets_lock_daemon() {
    printf '%s\n' "$$" >"$lock_daemon_pid"
    trap 'rm -f "$lock_daemon_pid"' EXIT

    while true; do
        ensure_config
        widgets_locked || break
        sync_locked_widgets
        sleep 0.25
    done
}

run_widget_delete_watcher() {
    local id addr label
    declare -A seen

    printf '%s\n' "$$" >"$delete_watcher_pid"
    trap 'rm -f "$delete_watcher_pid"' EXIT

    while true; do
        ensure_config
        widgets_locked && break
        [[ -f "$managed_stop_marker" ]] && break

        for id in $(custom_widget_ids); do
            [[ -n "$id" ]] || continue
            addr="$(widget_client_address "$id")"
            if [[ -n "$addr" ]]; then
                seen["$id"]=1
                continue
            fi

            if [[ "${seen[$id]:-0}" == "1" ]]; then
                label="$(custom_widget_meta "$id" name 2>/dev/null || printf '%s' "$id")"
                remove_custom_widget "$id" >/dev/null 2>&1 || true
                unset "seen[$id]"
                notify "Widget eliminato: $label"
            fi
        done

        sleep 1
    done
}

apply_widgets_lock_state() {
    local name addr value zorder

    ensure_config
    if widgets_locked; then
        value=1
        zorder=bottom
    else
        value=0
        zorder=top
    fi

    for name in $(widget_order_default); do
        [[ -n "$name" ]] || continue
        addr="$(widget_client_address "$name")"
        [[ -n "$addr" ]] || continue
        hyprctl setprop "address:$addr" no_focus "$value" >/dev/null 2>&1 ||
            hyprctl setprop "address:$addr" nofocus "$value" >/dev/null 2>&1 ||
            true
        hyprctl setprop "address:$addr" no_follow_mouse "$value" >/dev/null 2>&1 ||
            hyprctl setprop "address:$addr" nofollowmouse "$value" >/dev/null 2>&1 ||
            true
        hyprctl setprop "address:$addr" focus_on_activate 0 >/dev/null 2>&1 || true
        hyprctl dispatch alterzorder "$zorder,address:$addr" >/dev/null 2>&1 || true
    done
}

apply_widget_layout_locked() {
    local status=0

    apply_widget_layout || status=$?
    apply_widgets_lock_state
    return "$status"
}

launch_named_widget() {
    local name="$1"

    case "$name" in
        clock)
            launch_widget clock "$(widget_meta clock class)" "$(widget_meta clock title)" "$clock_command"
            ;;
        cava)
            launch_widget cava "$(widget_meta cava class)" "$(widget_meta cava title)" "$cava_command"
            ;;
        system)
            launch_widget system "$(widget_meta system class)" "$(widget_meta system title)" "$system_command"
            ;;
        *)
            launch_custom_widget "$name"
            ;;
    esac
}

show_named_widget() {
    local name="$1"
    local addr label

    ensure_config
    layout_load
    label="$(widget_label "$name")"

    addr="$(widget_client_address "$name")"
    if [[ -z "$addr" ]]; then
        launch_named_widget "$name" || {
            notify "Widget non avviato: $label"
            return 1
        }
    fi

    if ! apply_widget_layout_locked; then
        layout_default_stack
        apply_widget_layout_locked
    fi

    addr="$(widget_client_address "$name")"
    if [[ -n "$addr" && ! widgets_locked ]]; then
        hyprctl dispatch alterzorder "top,address:$addr" >/dev/null 2>&1 || true
    fi

    notify "Widget pronto: $label"
}

set_widgets_lock() {
    local state="$1"

    case "$state" in
        lock | locked | 1 | true)
            stop_widget_delete_watcher
            save_widget_layout 2>/dev/null || true
            set_config_value ANTO426_WIDGETS_LOCKED 1
            write_widget_lock_hypr_rules lock
            apply_widgets_lock_state
            sync_locked_widgets
            start_widgets_lock_daemon
            notify "Widget bloccati"
            ;;
        unlock | unlocked | 0 | false)
            stop_widgets_lock_daemon
            set_config_value ANTO426_WIDGETS_LOCKED 0
            write_widget_lock_hypr_rules unlock
            apply_widgets_lock_state
            apply_widget_layout >/dev/null 2>&1 || true
            start_widget_delete_watcher
            notify "Widget sbloccati"
            ;;
        toggle)
            if widgets_locked; then
                set_widgets_lock unlock
            else
                set_widgets_lock lock
            fi
            ;;
        *)
            return 1
            ;;
    esac
}

widget_label() {
    local name="$1"
    local label

    case "$name" in
        clock) printf '󰥔 Orologio' ;;
        cava) printf '󰎈 Musica' ;;
        system) printf '󰍛 Sistema' ;;
        *)
            label="$(custom_widget_meta "$name" name 2>/dev/null || printf '%s' "$name")"
            printf '󰧖 %s' "$label"
            ;;
    esac
}

select_widget_from_order() {
    local theme="$1"
    local prompt="$2"
    local order choice name

    order="$(widget_order_default)"
    choice="$(
        for name in $order; do
            [[ -n "$name" ]] && widget_label "$name" && printf '\n'
        done | rofi -dmenu -i -p "$prompt" -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    for name in $order; do
        [[ "$choice" == "$(widget_label "$name")" ]] && {
            printf '%s' "$name"
            return 0
        }
    done

    return 1
}

widget_name_from_label() {
    local label="$1"
    local name

    for name in $(widget_order_default); do
        [[ "$label" == "$(widget_label "$name")" ]] && {
            printf '%s' "$name"
            return 0
        }
    done

    return 1
}

write_widget_order() {
    local order="$1"
    local file tmp

    file="$(widget_layout_file)"
    tmp="$(mktemp)"
    awk -v order="$order" '
        /^export ANTO426_WIDGET_ORDER=/ {
            print "export ANTO426_WIDGET_ORDER=\"" order "\""
            next
        }
        { print }
    ' "$file" >"$tmp" && mv "$tmp" "$file"
    # shellcheck disable=SC1090
    source "$file"
}

move_widget_in_order() {
    local name="$1"
    local direction="$2"
    local order idx count i tmp new_order
    local -a items

    order="$(widget_order_default)"
    read -ra items <<<"$order"
    count="${#items[@]}"
    for ((i = 0; i < count; i++)); do
        [[ "${items[i]}" == "$name" ]] && idx=$i && break
    done

    case "$direction" in
        up)
            [[ -n "${idx:-}" && "$idx" -gt 0 ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx - 1]}"
            items[idx - 1]="$tmp"
            ;;
        down)
            [[ -n "${idx:-}" && "$idx" -lt $((count - 1)) ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx + 1]}"
            items[idx + 1]="$tmp"
            ;;
        *)
            return 1
            ;;
    esac

    new_order="${items[*]}"
    write_widget_order "$new_order"
    layout_default_stack
    apply_widget_layout_locked
    notify "Ordine aggiornato"
}

arrange_widgets() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local order choice name new_id selected

    ensure_config
    layout_load
    order="$(widget_order_default)"

    choice="$(
        {
            printf '%s\n' "󱓞 Mostra/nasconde widget"
            printf '%s\n' "󰑓 Riavvia widget"
            if widgets_locked; then
                printf '%s\n' "󰌾 Sblocca widget"
            else
                printf '%s\n' "󰌾 Blocca widget"
            fi
            printf '%s\n' "󰒓 Applica layout salvato"
            printf '%s\n' "󰑐 Reset posizioni"
            printf '%s\n' "󰆓 Salva posizioni attuali"
            printf '%s\n' "󰐕 Aggiungi widget terminale"
            printf '%s\n' "󰣖 Aggiungi widget da app"
            printf '%s\n' "󰅙 Rimuovi widget"
            printf '%s\n' "── Ordine ──"
            for name in $order; do
                [[ -n "$name" ]] && widget_label "$name" && printf '\n'
            done
            printf '%s\n' "󰅖 Sposta selezionato su"
            printf '%s\n' "󰅁 Sposta selezionato giù"
        } | rofi -dmenu -i -matching fuzzy -p "Widget" \
            -mesg "Trascina: Super + click sinistro\nSalva: Super + Alt + G\nNuovo terminale: scrivi qualsiasi comando/processo" -theme "$theme"
    )"

    [[ -z "$choice" ]] && return 0

    case "$choice" in
        *"Mostra/nasconde"*)
            if any_running; then
                stop_widgets
            else
                start_widgets
            fi
            ;;
        *"Riavvia widget"*)
            stop_widgets quiet
            sleep 0.3
            start_widgets
            ;;
        *"Blocca widget"*) set_widgets_lock lock ;;
        *"Sblocca widget"*) set_widgets_lock unlock ;;
        *"Applica layout"*) apply_widget_layout_locked && notify "Layout applicato" ;;
        *"Reset posizioni"*) reset_widget_layout && apply_widgets_lock_state && notify "Layout resettato" ;;
        *"Salva posizioni"*) save_widget_layout && apply_widget_layout_locked && notify "Layout salvato" ;;
        *"Aggiungi widget terminale"*)
            if new_id="$(pick_terminal_widget)"; then
                launch_custom_widget "$new_id"
                layout_default_stack
                apply_widget_layout_locked
                notify "Widget terminale aggiunto"
            fi
            ;;
        *"Aggiungi widget da app"*)
            if new_id="$(pick_app_for_widget)"; then
                launch_custom_widget "$new_id"
                layout_default_stack
                apply_widget_layout_locked
                notify "Widget app aggiunto"
            fi
            ;;
        *"Rimuovi widget"*)
            selected="$(select_widget_from_order "$theme" "Rimuovi widget")" || return 0
            if widget_is_builtin "$selected"; then
                notify "I widget base si disattivano da widgets.env"
                return 0
            fi
            remove_custom_widget "$selected"
            layout_default_stack
            apply_widget_layout_locked
            notify "Widget rimosso"
            ;;
        *"Sposta selezionato su")
            selected="$(select_widget_from_order "$theme" "Quale widget")" || return 0
            move_widget_in_order "$selected" up
            ;;
        *"Sposta selezionato giù")
            selected="$(select_widget_from_order "$theme" "Quale widget")" || return 0
            move_widget_in_order "$selected" down
            ;;
        "── Ordine ──") ;;
        *)
            if selected="$(widget_name_from_label "$choice")"; then
                show_named_widget "$selected"
            fi
            ;;
    esac
}

case "${1:-toggle}" in
    autostart)
        [[ "${ANTO426_WIDGETS_AUTOSTART:-1}" == "0" ]] && exit 0
        start_widgets quiet
        ;;
    start) start_widgets ;;
    stop) stop_widgets ;;
    restart)
        stop_widgets quiet
        sleep 0.3
        start_widgets
        ;;
    reload)
        stop_widgets quiet
        sleep 0.3
        start_widgets quiet
        ;;
    toggle)
        if any_running; then
            stop_widgets
        else
            start_widgets
        fi
        ;;
    save-layout) save_widget_layout && apply_widget_layout_locked && notify "Layout salvato" ;;
    arrange) arrange_widgets ;;
    lock) set_widgets_lock lock ;;
    unlock) set_widgets_lock unlock ;;
    toggle-lock) set_widgets_lock toggle ;;
    lock-daemon) run_widgets_lock_daemon ;;
    delete-watcher) run_widget_delete_watcher ;;
    status)
        if any_running; then
            printf 'running\n'
        else
            printf 'stopped\n'
        fi
        ;;
    *)
        printf 'Uso: %s [autostart|start|stop|restart|reload|toggle|save-layout|arrange|lock|unlock|toggle-lock|status]\n' "$0" >&2
        exit 2
        ;;
esac
