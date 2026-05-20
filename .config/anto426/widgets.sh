#!/usr/bin/env bash
set -uo pipefail

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-widgets"
mkdir -p "$runtime_dir"

notify() {
    notify-send "Widget" "$*" 2>/dev/null || true
}

terminal_cmd() {
    if command -v ghostty >/dev/null 2>&1; then
        printf 'ghostty'
        return 0
    fi
    return 1
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
        notify "Terminale ghostty non trovato"
        return 1
    }

    "$terminal" --class="$class" --title="$title" -e bash -lc "$command" >/dev/null 2>&1 &
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
    pkill -f -- "--class=$class" 2>/dev/null || true
    rm -f "$pid_file"
}

clock_command='
while true; do
    clear
    printf "\n"
    printf "   %s\n" "$(date +%H:%M)"
    printf "   %s\n" "$(date "+%A %d %B")"
    printf "\n   %s\n" "$(date "+%Y")"
    sleep 1
done
'

cava_command='
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
    printf "\n   Sistema\n\n"
    printf "   BAT  %s\n" "$battery"
    printf "   RAM  %s\n" "${mem:-n/d}"
    printf "   CPU  %s\n" "${load:-n/d}"
    printf "   VOL  %s\n" "${volume:-n/d}"
    printf "   LUX  %s\n" "${brightness:-n/d}"
    sleep 5
done
'

start_widgets() {
    [[ "${ANTO426_WIDGETS_AUTOSTART:-1}" == "0" ]] && return 0
    launch_widget clock clock-widget "Clock Widget" "$clock_command"
    launch_widget cava cava-widget "Cava Widget" "$cava_command"
    launch_widget system system-widget "System Widget" "$system_command"
    notify "Widget avviati"
}

stop_widgets() {
    stop_widget clock clock-widget
    stop_widget cava cava-widget
    stop_widget system system-widget
    notify "Widget chiusi"
}

any_running() {
    is_running clock || is_running cava || is_running system
}

case "${1:-toggle}" in
    start) start_widgets ;;
    stop) stop_widgets ;;
    restart)
        stop_widgets
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
        printf 'Uso: %s [start|stop|restart|toggle|status]\n' "$0" >&2
        exit 2
        ;;
esac
