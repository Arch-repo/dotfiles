#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=widgets_layout.sh
source "$SCRIPT_DIR/widgets_layout.sh"

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
config_file="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets.env"
cava_config="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/cava_widget.conf"
colors_file="${XDG_CONFIG_HOME:-$HOME/.config}/colors/colors.sh"
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
    if layout_needs_defaults; then
        layout_default_stack
    fi

    pkill -f -- ".config/anto426/desktop_widgets.py" 2>/dev/null || true

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

    (
        apply_widget_layout || layout_default_stack && apply_widget_layout
    ) &

    [[ "$quiet" == "quiet" ]] || notify "Widget avviati — trascina con Super + tasto sinistro"
}

stop_widgets() {
    local quiet="${1:-false}"

    save_widget_layout 2>/dev/null || true

    pkill -f -- ".config/anto426/desktop_widgets.py" 2>/dev/null || true
    stop_widget clock "$(widget_meta clock class)"
    stop_widget cava "$(widget_meta cava class)"
    stop_widget system "$(widget_meta system class)"
    [[ "$quiet" == "quiet" ]] || notify "Widget chiusi — layout salvato"
}

any_running() {
    is_running clock || is_running cava || is_running system
}

arrange_widgets() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local order choice name file tmp new_order line selected idx count i

    ensure_config
    layout_load
    order="$(widget_order_default)"
    file="$(widget_layout_file)"

    choice="$(
        {
            printf '%s\n' "󰒓 Applica layout salvato"
            printf '%s\n' "󰑐 Reset posizioni"
            printf '%s\n' "󰆓 Salva posizioni attuali"
            printf '%s\n' "── Ordine ──"
            for name in $order; do
                case "$name" in
                    clock) printf '󰥔 Orologio\n' ;;
                    cava) printf '󰎈 Musica\n' ;;
                    system) printf '󰍛 Sistema\n' ;;
                esac
            done
            printf '%s\n' "󰅖 Sposta selezionato su"
            printf '%s\n' "󰅁 Sposta selezionato giù"
        } | rofi -dmenu -i -matching fuzzy -p "Widget" \
            -mesg "Trascina: Super + click sinistro\nSalva: Super + Alt + G" -theme "$theme"
    )"

    [[ -z "$choice" ]] && return 0

    case "$choice" in
        *"Applica layout"*) apply_widget_layout && notify "Layout applicato" ;;
        *"Reset posizioni"*) reset_widget_layout && notify "Layout resettato" ;;
        *"Salva posizioni"*) save_widget_layout && notify "Layout salvato" ;;
        *"Sposta selezionato su")
            selected="$(
                for name in $order; do
                    case "$name" in
                        clock) printf '󰥔 Orologio\n' ;;
                        cava) printf '󰎈 Musica\n' ;;
                        system) printf '󰍛 Sistema\n' ;;
                    esac
                done | rofi -dmenu -i -p "Quale widget" -theme "$theme"
            )"
            [[ -z "$selected" ]] && return 0
            case "$selected" in
                *Orologio) name=clock ;;
                *Musica*) name=cava ;;
                *) name=system ;;
            esac
            read -ra items <<<"$order"
            count="${#items[@]}"
            for ((i = 0; i < count; i++)); do
                [[ "${items[i]}" == "$name" ]] && idx=$i && break
            done
            [[ -n "${idx:-}" && "$idx" -gt 0 ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx - 1]}"
            items[idx - 1]="$tmp"
            new_order="${items[*]}"
            tmp="$(mktemp)"
            awk -v order="$new_order" '
                /^export ANTO426_WIDGET_ORDER=/ {
                    print "export ANTO426_WIDGET_ORDER=\"" order "\""
                    next
                }
                { print }
            ' "$file" >"$tmp" && mv "$tmp" "$file"
            # shellcheck disable=SC1090
            source "$file"
            layout_default_stack
            apply_widget_layout
            notify "Ordine aggiornato"
            ;;
        *"Sposta selezionato giù")
            selected="$(
                for name in $order; do
                    case "$name" in
                        clock) printf '󰥔 Orologio\n' ;;
                        cava) printf '󰎈 Musica\n' ;;
                        system) printf '󰍛 Sistema\n' ;;
                    esac
                done | rofi -dmenu -i -p "Quale widget" -theme "$theme"
            )"
            [[ -z "$selected" ]] && return 0
            case "$selected" in
                *Orologio) name=clock ;;
                *Musica*) name=cava ;;
                *) name=system ;;
            esac
            read -ra items <<<"$order"
            count="${#items[@]}"
            for ((i = 0; i < count; i++)); do
                [[ "${items[i]}" == "$name" ]] && idx=$i && break
            done
            [[ -n "${idx:-}" && "$idx" -lt $((count - 1)) ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx + 1]}"
            items[idx + 1]="$tmp"
            new_order="${items[*]}"
            tmp="$(mktemp)"
            awk -v order="$new_order" '
                /^export ANTO426_WIDGET_ORDER=/ {
                    print "export ANTO426_WIDGET_ORDER=\"" order "\""
                    next
                }
                { print }
            ' "$file" >"$tmp" && mv "$tmp" "$file"
            # shellcheck disable=SC1090
            source "$file"
            layout_default_stack
            apply_widget_layout
            notify "Ordine aggiornato"
            ;;
        "── Ordine ──") ;;
        *) ;;
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
        start_widgets
        ;;
    toggle)
        if any_running; then
            stop_widgets
        else
            start_widgets
        fi
        ;;
    save-layout) save_widget_layout && notify "Layout salvato" ;;
    arrange) arrange_widgets ;;
    status)
        if any_running; then
            printf 'running\n'
        else
            printf 'stopped\n'
        fi
        ;;
    *)
        printf 'Uso: %s [autostart|start|stop|restart|reload|toggle|save-layout|arrange|status]\n' "$0" >&2
        exit 2
        ;;
esac
