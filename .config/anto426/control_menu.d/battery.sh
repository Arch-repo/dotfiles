#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

battery_path() {
    local supply
    for supply in /sys/class/power_supply/*; do
        [[ -r "$supply/type" ]] || continue
        [[ "$(cat "$supply/type" 2>/dev/null)" == "Battery" ]] || continue
        printf '%s' "$supply"
        return 0
    done
    return 1
}

power_online() {
    local supply
    for supply in /sys/class/power_supply/*; do
        [[ -r "$supply/type" && -r "$supply/online" ]] || continue
        [[ "$(cat "$supply/type" 2>/dev/null)" == "Mains" ]] || continue
        [[ "$(cat "$supply/online" 2>/dev/null)" == "1" ]] && {
            system_text "Connected"
            return 0
        }
    done
    system_text "Disconnected"
}

battery_read() {
    local path="$1"
    local field="$2"
    [[ -r "$path/$field" ]] && cat "$path/$field" 2>/dev/null
}

battery_status_label() {
    case "$1" in
        Charging) system_text "Charging" ;;
        Discharging) system_text "Discharging" ;;
        Full) system_text "Full" ;;
        "Not charging") system_text "Not charging" ;;
        *) printf '%s' "${1:-$(system_text "Unknown")}" ;;
    esac
}

battery_profile() {
    if command -v powerprofilesctl >/dev/null 2>&1; then
        powerprofilesctl get 2>/dev/null || system_text "Unavailable"
    else
        system_text "Unavailable"
    fi
}

battery_time_estimate() {
    local path="$1"
    local status="$2"
    local now full rate minutes

    if [[ -r "$path/energy_now" && -r "$path/power_now" ]]; then
        now="$(battery_read "$path" energy_now)"
        full="$(battery_read "$path" energy_full)"
        rate="$(battery_read "$path" power_now)"
    else
        now="$(battery_read "$path" charge_now)"
        full="$(battery_read "$path" charge_full)"
        rate="$(battery_read "$path" current_now)"
    fi

    [[ "$rate" =~ ^[0-9]+$ && "$rate" -gt 0 ]] || return 0
    [[ "$now" =~ ^[0-9]+$ ]] || return 0

    if [[ "$status" == "Charging" && "$full" =~ ^[0-9]+$ && "$full" -gt "$now" ]]; then
        now=$((full - now))
    elif [[ "$status" != "Discharging" ]]; then
        return 0
    fi

    minutes="$(awk -v left="$now" -v rate="$rate" 'BEGIN {printf "%d", (left / rate) * 60}')"
    ((minutes > 0)) || return 0
    printf '%dh %02dm' "$((minutes / 60))" "$((minutes % 60))"
}

battery_health() {
    local path="$1"
    local full design
    full="$(battery_read "$path" charge_full)"
    design="$(battery_read "$path" charge_full_design)"

    if [[ -z "$full" || -z "$design" ]]; then
        full="$(battery_read "$path" energy_full)"
        design="$(battery_read "$path" energy_full_design)"
    fi

    if [[ "$full" =~ ^[0-9]+$ && "$design" =~ ^[0-9]+$ && "$design" -gt 0 ]]; then
        awk -v full="$full" -v design="$design" 'BEGIN {printf "%d%%", (full / design) * 100}'
    else
        system_text "Unavailable"
    fi
}

battery_message() {
    local path="$1"
    local capacity status status_label estimate profile health manufacturer model power_state
    capacity="$(battery_read "$path" capacity)"
    status="$(battery_read "$path" status)"
    status_label="$(battery_status_label "$status")"
    estimate="$(battery_time_estimate "$path" "$status")"
    profile="$(battery_profile)"
    health="$(battery_health "$path")"
    manufacturer="$(battery_read "$path" manufacturer)"
    model="$(battery_read "$path" model_name)"
    power_state="$(power_online)"

    # dynamic color mapping
    local cap_color="$c_green"
    if [[ -n "$capacity" && "$capacity" =~ ^[0-9]+$ ]]; then
        if (( capacity < 20 )); then
            cap_color="$c_red"
        elif (( capacity < 50 )); then
            cap_color="$c_yellow"
        fi
    fi
    
    local status_color="$c_muted"
    [[ "$status" == "Charging" ]] && status_color="$c_green"
    [[ "$status" == "Full" ]] && status_color="$c_cyan"

    local out
    out="<b>$(system_text "Battery")</b>: <span foreground='${cap_color}'>${capacity:-?}%</span>\n"
    out="${out}<b>$(system_text "Status")</b>: <span foreground='${status_color}'>${status_label}</span>\n"
    out="${out}<b>$(system_text "Power")</b>: <span foreground='${c_yellow}'>${power_state}</span>"
    
    if [[ -n "$estimate" ]]; then
        out="${out}\n<b>$(system_text "Time")</b>: <span foreground='${c_cyan}'>${estimate}</span>"
    fi
    
    out="${out}\n<b>$(system_text "Profile")</b>: <span foreground='${c_accent}'>${profile}</span>"
    out="${out}\n<b>$(system_text "Health")</b>: <span foreground='${c_accent}'>${health}</span>"
    
    if [[ -n "$manufacturer$model" ]]; then
        out="${out}\n<span foreground='${c_muted}'>${manufacturer} ${model}</span>"
    fi
    
    printf '%b' "$out"
}

battery_details_message() {
    local path="$1"
    local upower_device

    if command -v upower >/dev/null 2>&1; then
        upower_device="$(upower -e 2>/dev/null | awk '/battery|BAT/ {print; exit}')"
        if [[ -n "$upower_device" ]]; then
            upower -i "$upower_device" 2>/dev/null |
                awk -F: '
                    /native-path|vendor|model|serial|power supply|updated|state|warning-level|energy:|energy-full:|energy-full-design:|energy-rate|voltage|charge-cycles|percentage|capacity|technology/ {
                        key = $1
                        value = $0
                        sub(/^[^:]+:[[:space:]]*/, "", value)
                        gsub(/^[[:space:]]+|[[:space:]]+$/, "", key)
                        printf "<b>%s</b>: %s\n", key, value
                    }
                '
            return 0
        fi
    fi

    for field in manufacturer model_name serial_number technology status capacity cycle_count voltage_now energy_now energy_full energy_full_design charge_now charge_full charge_full_design power_now current_now; do
        [[ -r "$path/$field" ]] || continue
        printf '<b>%s</b>: %s\n' "$field" "$(battery_read "$path" "$field")"
    done | sed -n '1,18p'
}

battery_menu() {
    local path
    path="$(battery_path)" || {
        local choice
        choice="$(
            printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")" |
                rofi_pick_msg "$(system_text "Battery")" "$(system_text "No battery detected")"
        )"
        [[ -n "$choice" ]] && back_or_main
        return 0
    }

    while true; do
        local choice message title profile
        message="$(battery_message "$path")"
        title="$(system_text "Battery")"
        profile="$(battery_profile)"

        local active_saver active_balanced active_perf
        active_saver=""
        [[ "$profile" == "power-saver" ]] && active_saver=" ($(system_text "Active"))"
        active_balanced=""
        [[ "$profile" == "balanced" ]] && active_balanced=" ($(system_text "Active"))"
        active_perf=""
        [[ "$profile" == "performance" ]] && active_perf=" ($(system_text "Active"))"

        choice="$(
            {
                if command -v powerprofilesctl >/dev/null 2>&1; then
                    printf '%s%s\0icon\x1fbattery-low\n' "$(menu_item "󰌪" "Power Saver Profile")" "$active_saver"
                    printf '%s%s\0icon\x1fbattery-good\n' "$(menu_item "󰗑" "Balanced Profile")" "$active_balanced"
                    printf '%s%s\0icon\x1fbattery-full\n' "$(menu_item "󰓅" "Performance Profile")" "$active_perf"
                fi
                printf '%s\0icon\x1fview-refresh\n' "$(menu_item "󰑐" "Refresh Status")"
                printf '%s\0icon\x1fdialog-information\n' "$(menu_item "󰄧" "Battery Details")"
                printf '%s\0icon\x1fsystem-suspend\n' "$(menu_item "󰤄" "Suspend System")"
                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "$title" "$message"
        )"

        [[ -z "$choice" ]] && return 0
        local clean_choice="$choice"

        case "$clean_choice" in
            "$(system_text "Power Saver Profile")"*) run_or_notify "$(system_text "Power Saver")" powerprofilesctl set power-saver ;;
            "$(system_text "Balanced Profile")"*) run_or_notify "$(system_text "Balanced")" powerprofilesctl set balanced ;;
            "$(system_text "Performance Profile")"*) run_or_notify "$(system_text "Performance")" powerprofilesctl set performance ;;
            "$(system_text "Refresh Status")") continue ;;
            "$(system_text "Battery Details")") rofi_pick_msg "Battery Details" "$(battery_details_message "$path")" >/dev/null ;;
            "$(system_text "Suspend System")") systemctl suspend; return 0 ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
        esac
    done
}

battery_menu
