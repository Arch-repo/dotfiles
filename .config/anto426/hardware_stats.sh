#!/usr/bin/env bash
set -uo pipefail

shopt -s nullglob

runtime_dir="${XDG_RUNTIME_DIR:-/tmp}"
cpu_cache="$runtime_dir/anto426-hardware-stats.cpu"
theme_menu="$HOME/.config/rofi/control_menu.rasi"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

read_cpu_totals() {
    local label user nice system idle iowait irq softirq steal guest guest_nice
    read -r label user nice system idle iowait irq softirq steal guest guest_nice < /proc/stat
    local idle_all=$((idle + iowait))
    local total=$((user + nice + system + idle + iowait + irq + softirq + steal))
    printf '%s %s\n' "$total" "$idle_all"
}

cpu_usage() {
    local total idle prev_total prev_idle diff_total diff_idle usage

    read -r total idle < <(read_cpu_totals)

    if [[ -r "$cpu_cache" ]]; then
        read -r prev_total prev_idle < "$cpu_cache" || true
    fi

    printf '%s %s\n' "$total" "$idle" > "$cpu_cache"

    if [[ ! "${prev_total:-}" =~ ^[0-9]+$ || ! "${prev_idle:-}" =~ ^[0-9]+$ ]]; then
        sleep 0.15
        read -r total idle < <(read_cpu_totals)
        read -r prev_total prev_idle < "$cpu_cache" || true
        printf '%s %s\n' "$total" "$idle" > "$cpu_cache"
    fi

    diff_total=$((total - prev_total))
    diff_idle=$((idle - prev_idle))

    if (( diff_total <= 0 )); then
        printf '0'
        return
    fi

    usage=$(((100 * (diff_total - diff_idle) + diff_total / 2) / diff_total))
    (( usage < 0 )) && usage=0
    (( usage > 100 )) && usage=100
    printf '%s' "$usage"
}

memory_stats() {
    awk '
        /^MemTotal:/ { total = $2 }
        /^MemAvailable:/ { available = $2 }
        END {
            if (total <= 0) {
                print "0 0.0 0.0"
                exit
            }
            used = total - available
            printf "%d %.1f %.1f\n", (used * 100 / total) + 0.5, used / 1048576, total / 1048576
        }
    ' /proc/meminfo
}

read_temp_c() {
    local path="$1"
    local milli

    [[ -r "$path" ]] || return 1
    milli="$(< "$path")"
    [[ "$milli" =~ ^-?[0-9]+$ ]] || return 1
    printf '%s' $(((milli + 500) / 1000))
}

cpu_temperature() {
    local dir name input label type temp

    for dir in /sys/class/hwmon/hwmon*; do
        [[ -r "$dir/name" ]] || continue
        name="$(< "$dir/name")"
        [[ "$name" == "coretemp" ]] || continue

        for input in "$dir"/temp*_input; do
            label="${input%_input}_label"
            [[ -r "$label" ]] || continue
            [[ "$(< "$label")" == "Package id 0" ]] || continue
            temp="$(read_temp_c "$input")" || continue
            printf '%s coretemp\n' "$temp"
            return
        done

        if [[ -r "$dir/temp1_input" ]]; then
            temp="$(read_temp_c "$dir/temp1_input")" || continue
            printf '%s coretemp\n' "$temp"
            return
        fi
    done

    for type in x86_pkg_temp TCPU; do
        for dir in /sys/class/thermal/thermal_zone*; do
            [[ -r "$dir/type" && -r "$dir/temp" ]] || continue
            [[ "$(< "$dir/type")" == "$type" ]] || continue
            temp="$(read_temp_c "$dir/temp")" || continue
            printf '%s %s\n' "$temp" "$type"
            return
        done
    done

    for dir in /sys/class/hwmon/hwmon*; do
        [[ -r "$dir/name" ]] || continue
        name="$(< "$dir/name")"
        [[ "$name" == "dell_ddv" || "$name" == "dell_smm" ]] || continue
        [[ -r "$dir/temp1_input" ]] || continue
        temp="$(read_temp_c "$dir/temp1_input")" || continue
        printf '%s %s\n' "$temp" "$name"
        return
    done

    printf '0 unknown\n'
}

fan_stats() {
    local dir name input idx rpm pwm enable mode fan_min fan_max fan_target note

    for dir in /sys/class/hwmon/hwmon*; do
        [[ -r "$dir/name" ]] || continue
        name="$(< "$dir/name")"

        for input in "$dir"/fan*_input; do
            [[ -r "$input" ]] || continue
            rpm="$(< "$input")"
            [[ "$rpm" =~ ^[0-9]+$ ]] || continue

            idx="${input##*/fan}"
            idx="${idx%_input}"
            [[ -e "$dir/pwm${idx}" && -e "$dir/pwm${idx}_enable" ]] || continue
            pwm="$(cat "$dir/pwm${idx}" 2>/dev/null || printf '?')"
            enable="$(cat "$dir/pwm${idx}_enable" 2>/dev/null || printf '?')"
            fan_min="$(cat "$dir/fan${idx}_min" 2>/dev/null || printf '?')"
            fan_max="$(cat "$dir/fan${idx}_max" 2>/dev/null || printf '?')"
            fan_target="$(cat "$dir/fan${idx}_target" 2>/dev/null || printf '?')"

            case "$enable" in
                0) mode="off" ;;
                1) mode="manual" ;;
                2) mode="auto" ;;
                3) mode="thermal" ;;
                *) mode="unknown" ;;
            esac

            note="raw"
            if [[ "$fan_max" =~ ^[0-9]+$ ]] && (( fan_max > 0 && rpm > fan_max )); then
                note="raw-over-max"
            fi

            printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
                "$rpm" "${name:-fan}" "$mode" "$pwm" "$fan_min" "$fan_max" "$fan_target" "$note"
            return
        done
    done

    for dir in /sys/class/hwmon/hwmon*; do
        [[ -r "$dir/name" ]] || continue
        name="$(< "$dir/name")"

        for input in "$dir"/fan*_input; do
            [[ -r "$input" ]] || continue
            rpm="$(< "$input")"
            [[ "$rpm" =~ ^[0-9]+$ ]] || continue
            idx="${input##*/fan}"
            idx="${idx%_input}"
            fan_min="$(cat "$dir/fan${idx}_min" 2>/dev/null || printf '?')"
            fan_max="$(cat "$dir/fan${idx}_max" 2>/dev/null || printf '?')"
            fan_target="$(cat "$dir/fan${idx}_target" 2>/dev/null || printf '?')"
            note="raw"
            if [[ "$fan_max" =~ ^[0-9]+$ ]] && (( fan_max > 0 && rpm > fan_max )); then
                note="raw-over-max"
            fi
            printf '%s\t%s\tunknown\t?\t%s\t%s\t%s\t%s\n' \
                "$rpm" "${name:-fan}" "$fan_min" "$fan_max" "$fan_target" "$note"
            return
        done
    done

    printf '0\tunavailable\tunknown\t?\t?\t?\t?\tunavailable\n'
}

fan_rpm_short() {
    local rpm="$1"
    local whole tenth

    [[ "$rpm" =~ ^[0-9]+$ ]] || {
        printf '?'
        return
    }

    if (( rpm >= 1000 )); then
        whole=$((rpm / 1000))
        tenth=$(((rpm % 1000 + 50) / 100))
        if (( tenth >= 10 )); then
            whole=$((whole + 1))
            tenth=0
        fi
        printf '~%d.%dk' "$whole" "$tenth"
    else
        printf '%s' "$rpm"
    fi
}

fan_control_paths() {
    local dir input idx pwm enable

    for dir in /sys/class/hwmon/hwmon*; do
        for input in "$dir"/fan*_input; do
            [[ -r "$input" ]] || continue
            idx="${input##*/fan}"
            idx="${idx%_input}"
            pwm="$dir/pwm${idx}"
            enable="$dir/pwm${idx}_enable"
            [[ -e "$pwm" && -e "$enable" ]] || continue
            printf '%s\t%s\n' "$pwm" "$enable"
            return 0
        done
    done

    return 1
}

root_write() {
    local path="$1"
    local value="$2"

    [[ "$value" =~ ^[0-9]+$ ]] || return 1
    case "$path" in
        /sys/class/hwmon/hwmon*/pwm[0-9]|/sys/class/hwmon/hwmon*/pwm[0-9]_enable)
            ;;
        *)
            return 1
            ;;
    esac

    if [[ -w "$path" ]]; then
        printf '%s\n' "$value" > "$path"
        return
    fi

    if command -v pkexec >/dev/null 2>&1; then
        pkexec bash -c 'printf "%s\n" "$1" > "$2"' bash "$value" "$path"
    elif command -v sudo >/dev/null 2>&1; then
        sudo bash -c 'printf "%s\n" "$1" > "$2"' bash "$value" "$path"
    else
        notify-send "Hardware" "Serve pkexec o sudo per controllare la ventola" 2>/dev/null || true
        return 1
    fi
}

clamp_pwm() {
    local raw="${1:-0}"

    raw="${raw//,/.}"
    awk -v value="$raw" '
        BEGIN {
            v = int((value + 0) + 0.5)
            if (v < 0) v = 0
            if (v > 255) v = 255
            printf "%d", v
        }
    '
}

set_fan_pwm() {
    local value="$1"
    local pwm_path enable_path requested applied rpm source mode pwm

    requested="$(clamp_pwm "$value")"

    # Se supportiamo i platform_profile, preferiamoli per i portatili moderni dove il controllo diretto PWM fallisce
    if [[ -f /sys/firmware/acpi/platform_profile ]] && command -v powerprofilesctl >/dev/null 2>&1; then
        local profile="balanced"
        if (( requested < 85 )); then
            profile="power-saver"
        elif (( requested >= 170 )); then
            profile="performance"
        fi
        if powerprofilesctl set "$profile"; then
            local rpm
            IFS=$'\t' read -r rpm _ < <(fan_stats)
            notify-send "Ventola" "Profilo $profile applicato. RPM: ${rpm}" 2>/dev/null || true
            return 0
        fi
    fi

    if ! IFS=$'\t' read -r pwm_path enable_path < <(fan_control_paths); then
        notify-send "Ventola" "Controllo PWM non disponibile" 2>/dev/null || true
        return 1
    fi

    if ! root_write "$enable_path" 1; then
        notify-send "Ventola" "Non riesco ad attivare il controllo manuale. Serve autorizzazione admin." 2>/dev/null || true
        return 1
    fi

    if ! root_write "$pwm_path" "$requested"; then
        notify-send "Ventola" "PWM non applicato. Serve autorizzazione admin o il firmware lo sta bloccando." 2>/dev/null || true
        return 1
    fi

    applied="$(cat "$pwm_path" 2>/dev/null || printf '?')"
    IFS=$'\t' read -r rpm source mode pwm _ _ _ _ < <(fan_stats)

    if [[ "$applied" == "$requested" ]]; then
        notify-send "Ventola" "PWM ${applied}/255 applicato. RPM: ${rpm}" 2>/dev/null || true
    else
        notify-send "Ventola" "Richiesto ${requested}/255, letto ${applied}/255. Firmware o permessi lo stanno limitando." 2>/dev/null || true
    fi
}


fan_slider() {
    local fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note value

    command -v rofi >/dev/null 2>&1 || {
        notify-send "Ventola" "Rofi non trovato" 2>/dev/null || true
        return 1
    }

    if ! IFS=$'\t' read -r _ _ < <(fan_control_paths); then
        notify-send "Ventola" "Controllo PWM non disponibile" 2>/dev/null || true
        return 1
    fi

    IFS=$'\t' read -r fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note < <(fan_stats)
    [[ "$fan_pwm" =~ ^[0-9]+$ ]] || fan_pwm=128

    # shellcheck source=rofi_slider.sh
    source "$script_dir/rofi_slider.sh"
    value="$(
        rofi_slider_pick \
            "slider-fan" \
            "Ventola" \
            "$(summary_message)\n\nNuovo PWM: {value}/255" \
            "$fan_pwm" 0 255 1 "" "control-slider-panel"
    )"
    [[ -z "$value" ]] && return 0
    set_fan_pwm "$value"
}

set_fan_profile() {
    local profile="$1"
    local pwm_path enable_path pwm_value label

    # Se supportiamo i platform_profile, usiamoli direttamente
    if [[ -f /sys/firmware/acpi/platform_profile ]] && command -v powerprofilesctl >/dev/null 2>&1; then
        local target_profile="balanced"
        case "$profile" in
            quiet|power-saver) target_profile="power-saver" ;;
            balanced|auto) target_profile="balanced" ;;
            performance|max) target_profile="performance" ;;
        esac
        if powerprofilesctl set "$target_profile"; then
            notify-send "Ventola" "Profilo termico $target_profile attivato" 2>/dev/null || true
            return 0
        fi
    fi

    if ! IFS=$'\t' read -r pwm_path enable_path < <(fan_control_paths); then
        notify-send "Ventola" "Controllo PWM non disponibile" 2>/dev/null || true
        return 1
    fi

    case "$profile" in
        auto)
            if root_write "$enable_path" 2; then
                notify-send "Ventola" "Profilo automatico attivato" 2>/dev/null || true
                return 0
            fi
            notify-send "Ventola" "Il firmware non ha accettato il profilo auto" 2>/dev/null || true
            return 1
            ;;
        quiet)
            pwm_value=145
            label="Quiet"
            ;;
        balanced)
            pwm_value=190
            label="Balanced"
            ;;
        performance)
            pwm_value=225
            label="Performance"
            ;;
        max)
            pwm_value=255
            label="Max"
            ;;
        *)
            return 1
            ;;
    esac

    root_write "$enable_path" 1 || return 1
    set_fan_pwm "$pwm_value" || return 1
    notify-send "Ventola" "Profilo $label (${pwm_value}/255)" 2>/dev/null || true
}

terminal_command() {
    local title="$1"
    local command="$2"
    local shell_name="${SHELL:-/bin/bash}"

    if command -v ghostty >/dev/null 2>&1; then
        ghostty --title="$title" -e bash -lc "$command; exec \"$shell_name\"" >/dev/null 2>&1 &
    elif command -v kitty >/dev/null 2>&1; then
        kitty --title "$title" bash -lc "$command; exec \"$shell_name\"" >/dev/null 2>&1 &
    elif command -v foot >/dev/null 2>&1; then
        foot --title "$title" bash -lc "$command; exec \"$shell_name\"" >/dev/null 2>&1 &
    elif command -v alacritty >/dev/null 2>&1; then
        alacritty --title "$title" -e bash -lc "$command; exec \"$shell_name\"" >/dev/null 2>&1 &
    else
        notify-send "Hardware" "Nessun terminale compatibile trovato" 2>/dev/null || true
        return 1
    fi
}

battery_summary() {
    local supply status capacity

    for supply in /sys/class/power_supply/*; do
        [[ -r "$supply/type" ]] || continue
        [[ "$(< "$supply/type")" == "Battery" ]] || continue
        capacity="$(cat "$supply/capacity" 2>/dev/null || printf '?')"
        status="$(cat "$supply/status" 2>/dev/null || printf 'Unknown')"
        printf '%s%% %s' "$capacity" "$status"
        return
    done

    printf 'N/A'
}

summary_message() {
    local cpu mem_percent mem_used mem_total temp temp_source load uptime battery
    local fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note fan_line

    cpu="$(cpu_usage)"
    read -r mem_percent mem_used mem_total < <(memory_stats)
    read -r temp temp_source < <(cpu_temperature)
    IFS=$'\t' read -r fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note < <(fan_stats)
    load="$(cut -d' ' -f1-3 /proc/loadavg 2>/dev/null || printf 'N/A')"
    uptime="$(uptime -p 2>/dev/null | sed 's/^up //' || printf 'N/A')"
    battery="$(battery_summary)"

    if [[ -f /sys/firmware/acpi/platform_profile ]]; then
        local current_prof
        current_prof="$(cat /sys/firmware/acpi/platform_profile 2>/dev/null || printf 'unknown')"
        fan_line="Ventola: ${fan_rpm} RPM (Profilo ACPI: $current_prof)"
    else
        fan_line="Fan raw: ${fan_rpm} RPM (${fan_source}, ${fan_mode}, pwm ${fan_pwm}/255"
        [[ "$fan_min" != "?" || "$fan_max" != "?" ]] && fan_line+=", min ${fan_min}, max ${fan_max}"
        [[ "$fan_target" != "?" ]] && fan_line+=", target ${fan_target}"
        fan_line+=")"
        if [[ "$fan_note" == "raw-over-max" ]]; then
            fan_line+=$'\n'"Fan note: firmware reports RPM above its own max, so this is shown as raw sensor data."
        fi
    fi

    printf 'CPU: %s%%\nRAM: %s GiB / %s GiB (%s%%)\nTemp: %s°C (%s)\n%s\nLoad: %s\nUptime: %s\nBattery: %s' \
        "$cpu" "$mem_used" "$mem_total" "$mem_percent" "$temp" "$temp_source" \
        "$fan_line" "$load" "$uptime" "$battery"
}

show_menu() {
    local choice process_label

    command -v rofi >/dev/null 2>&1 || {
        notify-send "Hardware" "$(summary_message)" 2>/dev/null || true
        return 0
    }

    if command -v btop >/dev/null 2>&1; then
        process_label="󰨇  Apri btop"
    else
        process_label="󰨇  Apri htop"
    fi

    choice="$(
        {
            printf '%s\n' "󰈐  Slider ventola"
            if [[ -f /sys/firmware/acpi/platform_profile ]]; then
                local current_prof
                current_prof="$(cat /sys/firmware/acpi/platform_profile 2>/dev/null || printf 'unknown')"
                printf '%s\n' "󰈐  Profilo: Silenzioso$([[ "$current_prof" == "quiet" || "$current_prof" == "power-saver" ]] && printf " (attivo)")"
                printf '%s\n' "󰈐  Profilo: Bilanciato$([[ "$current_prof" == "balanced" ]] && printf " (attivo)")"
                printf '%s\n' "󰈐  Profilo: Prestazioni$([[ "$current_prof" == "performance" ]] && printf " (attivo)")"
            else
                printf '%s\n' "󰈐  Ventola auto"
            fi
            printf '%s\n' "$process_label"
            printf '%s\n' "  Apri sensors"
            printf '%s\n' "󰌢  Apri fastfetch"
            printf '%s\n' "󰁹  Menu batteria"
        } | rofi -dmenu -i -matching fuzzy -p "Hardware" -mesg "$(summary_message)" -theme "$theme_menu"
    )" || return 0

    case "$choice" in
        *"Slider ventola"*) fan_slider ;;
        *"Ventola auto"*) set_fan_profile auto ;;
        *"Profilo: Silenzioso"*) set_fan_profile quiet ;;
        *"Profilo: Bilanciato"*) set_fan_profile balanced ;;
        *"Profilo: Prestazioni"*) set_fan_profile performance ;;
        *"btop"*) terminal_command "btop" "btop" ;;
        *"htop"*) terminal_command "htop" "htop" ;;
        *"sensors"*) terminal_command "sensors" "watch -n 1 sensors" ;;
        *"fastfetch"*) terminal_command "fastfetch" "fastfetch" ;;
        *"batteria"*) "$HOME/.config/anto426/control_menu.sh" battery >/dev/null 2>&1 & ;;
    esac
}

waybar_json() {
    local cpu mem_percent mem_used mem_total temp temp_source temp_icon class
    local fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note fan_short fan_tip

    cpu="$(cpu_usage)"
    read -r mem_percent mem_used mem_total < <(memory_stats)
    read -r temp temp_source < <(cpu_temperature)
    IFS=$'\t' read -r fan_rpm fan_source fan_mode fan_pwm fan_min fan_max fan_target fan_note < <(fan_stats)
    fan_short="$(fan_rpm_short "$fan_rpm")"

    temp_icon=""
    class="normal"
    if (( temp >= 85 )); then
        temp_icon=""
        class="critical"
    elif (( temp >= 75 )); then
        class="warning"
    fi

    if [[ -f /sys/firmware/acpi/platform_profile ]]; then
        local current_prof
        current_prof="$(cat /sys/firmware/acpi/platform_profile 2>/dev/null || printf 'unknown')"
        fan_tip="Ventola: ${fan_rpm} RPM (Profilo ACPI: $current_prof)"
    else
        fan_tip="Fan raw: ${fan_rpm} RPM (${fan_source}, ${fan_mode}, pwm ${fan_pwm}/255"
        [[ "$fan_min" != "?" || "$fan_max" != "?" ]] && fan_tip+=", min ${fan_min}, max ${fan_max}"
        [[ "$fan_target" != "?" ]] && fan_tip+=", target ${fan_target}"
        fan_tip+=")"
        if [[ "$fan_note" == "raw-over-max" ]]; then
            fan_tip+="\\nFan note: raw sensor is above firmware max"
        fi
    fi

    printf '{"text":"󰻠 %s%%  󰍛 %s%%  %s %s°C  󰈐 %s","tooltip":"CPU: %s%%\\nRAM: %s GiB / %s GiB (%s%%)\\nTemp: %s°C (%s)\\n%s","class":"%s"}\n' \
        "$cpu" "$mem_percent" "$temp_icon" "$temp" "$fan_short" \
        "$cpu" "$mem_used" "$mem_total" "$mem_percent" "$temp" "$temp_source" \
        "$fan_tip" "$class"
}

case "${1:-waybar}" in
    menu) show_menu ;;
    fan-slider) fan_slider ;;
    set-fan-pwm) set_fan_pwm "${2:-}" ;;
    fan-auto) set_fan_profile auto ;;
    btop)
        if command -v btop >/dev/null 2>&1; then
            terminal_command "btop" "btop"
        else
            terminal_command "htop" "htop"
        fi
        ;;
    sensors) terminal_command "sensors" "watch -n 1 sensors" ;;
    *) waybar_json ;;
esac
