#!/usr/bin/env bash
set -uo pipefail

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
config_file="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets.env"
mkdir -p "$runtime_dir"

ensure_config() {
    if [[ ! -f "$config_file" ]]; then
        cat >"$config_file" <<'EOF'
# Set to 0 to prevent widgets from starting with Hyprland.
export ANTO426_WIDGETS_AUTOSTART=1

# Available widgets: clock cava system
export ANTO426_WIDGETS_ENABLED="clock cava system"

# Terminal widgets are real floating windows, so you can move/resize them with Super + mouse.
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
            "$terminal" --class="$class" --title="$title" -e bash -lc "$command" >/dev/null 2>&1 &
            ;;
        kitty)
            "$terminal" --class "$class" --title "$title" bash -lc "$command" >/dev/null 2>&1 &
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
    local terminal

    is_running "$name" && return 0

    terminal="$(terminal_cmd)" || {
        notify "Terminale compatibile non trovato"
        return 1
    }

    if ! launch_terminal "$terminal" "$class" "$title" "$command"; then
        notify "Avvio widget fallito: $name"
        return 1
    fi
    printf '%s\n' "$!" >"$(pid_file "$name")"
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

clock_command='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
while true; do
    clear
    printf "\n   %s\n" "$(date +%H:%M)"
    printf "   %s\n" "$(date "+%a %d %b")"
    sleep 1
done
'

cava_command='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
if command -v cava >/dev/null 2>&1; then
    cava
else
    while true; do
        clear
        printf "\n   Audio visualizer\n\n"
        printf "   installa cava per il visualizzatore\n"
        sleep 5
    done
fi
'

system_command='
printf "\033[?25l"
trap "printf \"\033[?25h\"" EXIT
bar() {
    value="${1:-0}"
    filled=$((value / 10))
    out=""
    i=0
    while [ "$i" -lt 10 ]; do
        if [ "$i" -lt "$filled" ]; then out="${out}━"; else out="${out}─"; fi
        i=$((i + 1))
    done
    printf "%s" "$out"
}
while true; do
    clear
    battery="n/d"
    for b in /sys/class/power_supply/BAT*; do
        [ -r "$b/capacity" ] && battery="$(cat "$b/capacity")%" && break
    done
    mem="$(free -h 2>/dev/null | awk "/^Mem:/ {print \$3 \" / \" \$2}")"
    load="$(cut -d" " -f1-3 /proc/loadavg 2>/dev/null)"
    volume="$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | sed "s/^Volume: //")"
    brightness="$(brightnessctl -m 2>/dev/null | awk -F, "{print \$4; exit}")"
    brightness_value="$(printf "%s" "$brightness" | tr -dc "0-9" | sed -n "1p")"
    printf "\n   Sistema\n\n"
    printf "   BAT  %s\n" "$battery"
    printf "   RAM  %s\n" "${mem:-n/d}"
    printf "   CPU  %s\n" "${load:-n/d}"
    printf "   VOL  %s\n" "${volume:-n/d}"
    if [ -n "$brightness_value" ]; then
        printf "   LUX  %s %s\n" "${brightness:-n/d}" "$(bar "$brightness_value")"
    else
        printf "   LUX  %s\n" "${brightness:-n/d}"
    fi
    sleep 5
done
'

start_widgets() {
    local widget
    local quiet="${1:-false}"

    ensure_config

    # Clean up older layer-shell widget processes if this config was upgraded.
    pkill -f -- ".config/anto426/desktop_widgets.py" 2>/dev/null || true

    for widget in ${ANTO426_WIDGETS_ENABLED:-clock cava system}; do
        case "$widget" in
            clock) launch_widget clock clock-widget "Clock Widget" "$clock_command" ;;
            cava) launch_widget cava cava-widget "Cava Widget" "$cava_command" ;;
            system) launch_widget system system-widget "System Widget" "$system_command" ;;
        esac
    done

    [[ "$quiet" == "quiet" ]] || notify "Widget avviati"
}

stop_widgets() {
    local quiet="${1:-false}"

    pkill -f -- ".config/anto426/desktop_widgets.py" 2>/dev/null || true
    stop_widget clock clock-widget
    stop_widget cava cava-widget
    stop_widget system system-widget
    [[ "$quiet" == "quiet" ]] || notify "Widget chiusi"
}

any_running() {
    is_running clock || is_running cava || is_running system
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
    status)
        if any_running; then
            printf 'running\n'
        else
            printf 'stopped\n'
        fi
        ;;
    *)
        printf 'Uso: %s [autostart|start|stop|restart|reload|toggle|status]\n' "$0" >&2
        exit 2
        ;;
esac
