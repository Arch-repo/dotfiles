#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

audio_volume() {
    wpctl get-volume "$1" 2>/dev/null | sed 's/^Volume: //'
}

audio_muted_label() {
    wpctl get-volume "$1" 2>/dev/null | grep -q '\[MUTED\]' && printf 'sì' || printf 'no'
}

audio_volume_label() {
    local value muted
    value="$(audio_volume_percent "$1")"
    muted="$(audio_muted_label "$1")"
    [[ -n "$value" ]] || value=0
    if [[ "$muted" == "sì" ]]; then
        printf 'muto'
    else
        printf '%s%%' "$value"
    fi
}

audio_volume_percent() {
    wpctl get-volume "$1" 2>/dev/null | awk '
        /^Volume:/ {
            value = int(($2 * 100) + 0.5)
            if (value < 0) value = 0
            if (value > 100) value = 100
            print value
            exit
        }
    '
}

audio_volume_slider() {
    local target="$1"
    local prompt="$2"
    local message="$3"
    local slider_name="$4"
    local live_target="$5"
    local current value

    current="$(audio_volume_percent "$target")"
    [[ -n "$current" ]] || current=0

    value="$(rofi_slider_pick "$slider_name" "$prompt" "$message" "$current" 0 100 1 "$live_target")"
    [[ -z "$value" ]] && return 0
    ((value < 0)) && value=0
    ((value > 100)) && value=100
    wpctl set-volume -l 1 "$target" "${value}%" >/dev/null 2>&1 || true
}

audio_default_sink() {
    pactl info 2>/dev/null | awk -F': ' '/Default Sink:/ {print $2; exit}'
}

audio_default_source() {
    pactl info 2>/dev/null | awk -F': ' '/Default Source:/ {print $2; exit}'
}

audio_current_sink_desc() {
    local default
    default="$(audio_default_sink)"
    local desc
    desc="$(pactl list sinks 2>/dev/null | awk -v def="$default" '
        /^[[:space:]]Name: / {
            sub(/^[[:space:]]Name: /, "")
            name = $0
        }
        /^[[:space:]]Description: / && name == def {
            sub(/^[[:space:]]Description: /, "")
            print $0
            exit
        }
    ')"
    printf '%s' "${desc:-$default}"
}

audio_current_source_desc() {
    local default
    default="$(audio_default_source)"
    local desc
    desc="$(pactl list sources 2>/dev/null | awk -v def="$default" '
        /^[[:space:]]Name: / {
            sub(/^[[:space:]]Name: /, "")
            name = $0
        }
        /^[[:space:]]Description: / && name == def {
            sub(/^[[:space:]]Description: /, "")
            print $0
            exit
        }
    ')"
    printf '%s' "${desc:-$default}"
}

audio_choose_sink() {
    local default item sink
    default="$(audio_default_sink)"
    item="$(
        pactl list sinks 2>/dev/null | awk -v def="$default" '
            /^[[:space:]]Name: / {
                sub(/^[[:space:]]Name: /, "")
                name = $0
            }
            /^[[:space:]]Description: / {
                sub(/^[[:space:]]Description: /, "")
                active_suffix = (name == def) ? " (Default)" : ""
                printf "%s%s\t%s\0icon\x1faudio-card\n", $0, active_suffix, name
            }
        ' | rofi_pick "Output audio"
    )"
    sink="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$sink" ]] && run_or_notify "Output audio cambiato" pactl set-default-sink "$sink"
}

audio_choose_source() {
    local default item source
    default="$(audio_default_source)"
    item="$(
        pactl list sources 2>/dev/null | awk -v def="$default" '
            /^[[:space:]]Name: / {
                sub(/^[[:space:]]Name: /, "")
                name = $0
            }
            /^[[:space:]]Description: / && name !~ /\.monitor$/ {
                sub(/^[[:space:]]Description: /, "")
                active_suffix = (name == def) ? " (Default)" : ""
                printf "%s%s\t%s\0icon\x1faudio-input-microphone\n", $0, active_suffix, name
            }
        ' | rofi_pick "Input audio"
    )"
    source="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$source" ]] && run_or_notify "Input audio cambiato" pactl set-default-source "$source"
}

audio_mixer_command() {
    if command -v pwvucontrol >/dev/null 2>&1; then
        printf 'pwvucontrol'
    elif command -v pavucontrol >/dev/null 2>&1; then
        printf 'pavucontrol'
    fi
}

audio_menu() {
    while true; do
        local sink_desc source_desc sink_vol source_vol choice mixer_command
        sink_desc="$(audio_current_sink_desc)"
        source_desc="$(audio_current_source_desc)"
        sink_vol="$(audio_volume_label "@DEFAULT_AUDIO_SINK@")"
        source_vol="$(audio_volume_label "@DEFAULT_AUDIO_SOURCE@")"
        mixer_command="$(audio_mixer_command)"

        local message_card
        message_card="<b>OUTPUT</b>: ${sink_desc}\nVolume: <b><span foreground='${c_yellow}'>${sink_vol}</span></b>\n\n<b>INPUT</b>:  ${source_desc}\nVolume: <b><span foreground='${c_yellow}'>${source_vol}</span></b>"

        choice="$(
            {
                printf '%s\0icon\x1faudio-volume-muted\n' "$(menu_item "󰓃" "Mute/Unmute Output")"
                printf '%s (%d%%)\0icon\x1faudio-volume-high\n' "$(menu_item "󰓃" "Output Volume")" "$(audio_volume_percent "@DEFAULT_AUDIO_SINK@")"
                printf '%s\0icon\x1faudio-card\n' "$(menu_item "󰓃" "Select Output Device")"
                printf '%s\0icon\x1fmicrophone-sensitivity-muted\n' "$(menu_item "󰍬" "Mute/Unmute Microphone")"
                printf '%s (%d%%)\0icon\x1fmicrophone-sensitivity-high\n' "$(menu_item "󰍬" "Microphone Volume")" "$(audio_volume_percent "@DEFAULT_AUDIO_SOURCE@")"
                printf '%s\0icon\x1faudio-input-microphone\n' "$(menu_item "󰍬" "Select Input Device")"
                [[ -n "$mixer_command" ]] && printf '%s\0icon\x1fmultimedia-volume-control\n' "$(menu_item "󰓃" "Open Volume Mixer")"
                printf '%s\0icon\x1fdialog-information\n' "$(menu_item "󰄧" "Audio Diagnostics")"
                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "Audio" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        local clean_choice="$choice"

        case "$clean_choice" in
            "$(system_text "Mute/Unmute Output")") wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle ;;
            "$(system_text "Output Volume")"*)
                audio_volume_slider "@DEFAULT_AUDIO_SINK@" "Output Volume" \
                    "Output: $sink_desc" "slider-volume" "output-volume"
                ;;
            "$(system_text "Mute/Unmute Microphone")") wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle ;;
            "$(system_text "Microphone Volume")"*)
                audio_volume_slider "@DEFAULT_AUDIO_SOURCE@" "Microphone Volume" \
                    "Input: $source_desc" "slider-mic" "input-volume"
                ;;
            "$(system_text "Select Output Device")") audio_choose_sink ;;
            "$(system_text "Select Input Device")") audio_choose_source ;;
            "$(system_text "Open Volume Mixer")")
                [[ -n "$mixer_command" ]] && open_or_notify "Mixer audio" "$mixer_command"
                return 0
                ;;
            "$(system_text "Audio Diagnostics")")
                rofi_pick_msg "Audio Diagnostics" "$(pactl info 2>/dev/null)\n\nOutput: $sink_desc\nInput: $source_desc" >/dev/null
                ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
            *)
                ;;
        esac
    done
}

audio_menu

