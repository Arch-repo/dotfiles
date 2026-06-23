#!/usr/bin/env bash
[[ -n "${ANTO426_UTILS_LOADED:-}" ]] && return 0
ANTO426_UTILS_LOADED=1

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anto426"
EVENTS_FILE="$DATA_DIR/calendar/events.json"
GCAL_EVENTS_FILE="$DATA_DIR/calendar/google_events.json"
NOTIFICATIONS_FILE="$DATA_DIR/notifications/history.tsv"
WIFI_CACHE_DIR="$DATA_DIR/wifi"
WIFI_CACHE_FILE="$WIFI_CACHE_DIR/networks.tsv"
WIFI_CACHE_MAX_AGE=90
THEME_MENU="$HOME/.config/rofi/control_menu.rasi"
THEME_CALENDAR="$HOME/.config/rofi/control_calendar.rasi"
REMOTE_SYNC_SCRIPT="$HOME/.config/anto426/remote_sync.sh"

UTILS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Source the rofi slider script
source "$UTILS_DIR/../rofi_slider.sh"

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


# Global dynamic colors loaded dynamically from system configuration
c_accent="$(get_color accent "#8cb8e4")"
c_muted="$(get_color muted "#b9c4d2")"
c_yellow="$(get_color yellow "#f9e2af")"
c_green="$(get_color green "#a6e3a1")"
c_red="$(get_color red "#f38ba8")"
c_cyan="$(get_color cyan "#89dceb")"

system_locale() {
    local locale_name
    locale_name="${ANTO426_UI_LOCALE:-}"
    if [[ -z "$locale_name" ]]; then
        locale_name="$(
            localectl status 2>/dev/null |
                awk -F'LANG=' '/System Locale:/ {print $2; exit}'
        )"
    fi
    printf '%s' "${locale_name:-${LANG:-C.UTF-8}}"
}

UI_LOCALE="$(system_locale)"
SYSTEM_TEXT_DOMAINS=(
    gtk40
    gtk30
    NetworkManager
    blueman
    upower
    systemd
    gnome-shell
    gnome-control-center-2.0
)

system_text() {
    local msgid="$1"
    local domain translated

    if ! command -v gettext >/dev/null 2>&1; then
        printf '%s' "$msgid"
        return 0
    fi

    for domain in "${SYSTEM_TEXT_DOMAINS[@]}"; do
        translated="$(LC_ALL="$UI_LOCALE" gettext -d "$domain" "$msgid" 2>/dev/null || true)"
        if [[ -n "$translated" && "$translated" != "$msgid" ]]; then
            printf '%s' "$translated"
            return 0
        fi
    done

    printf '%s' "$msgid"
}

menu_item() {
    local icon="$1"
    local msgid="$2"
    printf '%s' "$(system_text "$msgid")"
}

rofi_pick() {
    local prompt="$1"
    local theme_or_str="${2:-}"
    local theme_str=""
    local theme="$THEME_MENU"

    if [[ -n "$theme_or_str" ]]; then
        if [[ -f "$theme_or_str" ]]; then
            theme="$theme_or_str"
        else
            theme_str="$theme_or_str"
        fi
    fi

    if [[ -n "$theme_str" ]]; then
        rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -theme-str "$theme_str" -theme "$theme"
    else
        rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -theme "$theme"
    fi
}

rofi_pick_msg() {
    local prompt="$1"
    local message="$2"
    local theme_or_str="${3:-}"
    local theme_arg="${4:-$THEME_MENU}"
    local theme_str=""
    local theme="$theme_arg"

    if [[ -n "$theme_or_str" ]]; then
        if [[ -f "$theme_or_str" ]]; then
            theme="$theme_or_str"
        else
            theme_str="$theme_or_str"
        fi
    fi

    # Expand any literal backslash escapes (like \n) into actual formatting newlines
    message="$(printf '%b' "$message")"

    if [[ -n "$theme_str" ]]; then
        rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -mesg "$message" -theme-str "$theme_str" -theme "$theme"
    else
        rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -mesg "$message" -theme "$theme"
    fi
}

rofi_input() {
    local prompt="$1"
    local value="${2:-}"
    printf '%s' "$value" | rofi -dmenu -show-icons -p "$prompt" -theme "$THEME_MENU"
}

rofi_password() {
    local prompt="$1"
    local message="${2:-}"

    if [[ -n "$message" ]]; then
        rofi -dmenu -password -show-icons -p "$prompt" -mesg "$message" -theme "$THEME_MENU"
    else
        rofi -dmenu -password -show-icons -p "$prompt" -theme "$THEME_MENU"
    fi
}

notify() {
    notify-send "Menu" "$*" 2>/dev/null || true
}

menu_clean_choice() {
    printf '%s' "$1" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//'
}

menu_is_action() {
    [[ "$1" == *"├─ "* || "$1" == *"└─ "* ]]
}

menu_is_back() {
    local clean
    clean="$(menu_clean_choice "$1")"
    [[ "$clean" == *"Back"* || "$clean" == *"Indietro"* ]]
}

menu_back_line() {
    printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
}

open_or_notify() {
    local label="$1"
    local command="$2"
    shift 2

    if command -v "$command" >/dev/null 2>&1; then
        "$command" "$@" >/dev/null 2>&1 &
        return 0
    fi

    notify "$label non disponibile"
    return 1
}

run_script_or_notify() {
    local label="$1"
    local script="$2"
    shift 2

    if [[ -x "$script" ]]; then
        "$script" "$@"
        return $?
    fi

    notify "$label non disponibile"
    return 1
}

run_or_notify() {
    local label="$1"
    shift
    local output

    if output="$("$@" 2>&1)"; then
        notify "$label"
        return 0
    fi

    notify "$label fallito${output:+: $output}"
    return 1
}

back_or_main() {
    MENU_STATE="main"
}
