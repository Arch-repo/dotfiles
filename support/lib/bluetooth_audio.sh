#!/usr/bin/env bash
[[ -n "${ANTO426_BT_AUDIO_LOADED:-}" ]] && return 0
ANTO426_BT_AUDIO_LOADED=1

bt_audio_mac_safe() {
    printf '%s' "$1" | tr ':' '_'
}

bt_audio_info_is_audio() {
    local info="$1"
    printf '%s\n' "$info" | grep -Eq 'UUID: (Audio Sink|Headset|Handsfree)|Icon: audio-(headset|headphones|card)'
}

bt_audio_battery_from_info() {
    local info="$1"
    printf '%s\n' "$info" | sed -n 's/.*Battery Percentage:.*(\([0-9][0-9]*\)).*/\1/p' | head -n1
}

bt_audio_card_for_mac() {
    local mac="$1"
    local safe
    safe="$(bt_audio_mac_safe "$mac")"
    pactl list cards short 2>/dev/null | awk -v card="bluez_card.${safe}" '$2 == card {print $2; exit}'
}

bt_audio_sink_for_mac() {
    local mac="$1"
    local safe
    safe="$(bt_audio_mac_safe "$mac")"
    pactl list sinks short 2>/dev/null | awk -v prefix="bluez_output.${safe}" 'index($2, prefix) == 1 {print $2; exit}'
}

bt_audio_source_for_mac() {
    local mac="$1"
    local safe
    safe="$(bt_audio_mac_safe "$mac")"
    pactl list sources short 2>/dev/null |
        awk -v colon="bluez_input.${mac}" -v safe="bluez_input.${safe}" '
            index($2, colon) == 1 || index($2, safe) == 1 {print $2; exit}
        '
}

bt_audio_active_profile() {
    local card="$1"
    [[ -n "$card" ]] || return 1
    pactl list cards 2>/dev/null | awk -v card="$card" '
        /^[[:space:]]Name: / {
            name = $0
            sub(/^[[:space:]]Name: /, "", name)
            in_card = (name == card)
        }
        in_card && /^[[:space:]]Active Profile: / {
            sub(/^[[:space:]]Active Profile: /, "")
            print
            exit
        }
    '
}

bt_audio_profile_label() {
    case "${1:-}" in
        a2dp-sink*) printf 'Hi-Fi' ;;
        headset-head-unit*) printf 'Mic' ;;
        off) printf 'off' ;;
        "") printf '' ;;
        *) printf '%s' "$1" ;;
    esac
}

bt_audio_profile_available() {
    local card="$1"
    local profile="$2"
    [[ -n "$card" && -n "$profile" ]] || return 1
    pactl list cards 2>/dev/null | awk -v card="$card" -v profile="$profile" '
        /^[[:space:]]Name: / {
            name = $0
            sub(/^[[:space:]]Name: /, "", name)
            in_card = (name == card)
        }
        in_card && $1 == profile ":" && /available: yes/ {
            found = 1
            exit
        }
        END {exit found ? 0 : 1}
    '
}

bt_audio_best_profile() {
    local card="$1"
    local mode="${2:-a2dp}"
    local profile

    if [[ "$mode" == "headset" ]]; then
        for profile in headset-head-unit headset-head-unit-cvsd; do
            bt_audio_profile_available "$card" "$profile" && {
                printf '%s' "$profile"
                return 0
            }
        done
    else
        for profile in a2dp-sink a2dp-sink-sbc_xq a2dp-sink-sbc; do
            bt_audio_profile_available "$card" "$profile" && {
                printf '%s' "$profile"
                return 0
            }
        done
    fi

    return 1
}

bt_audio_wait_for_card() {
    local mac="$1"
    local card=""
    local _i

    for _i in 1 2 3 4 5 6 7 8 9 10; do
        card="$(bt_audio_card_for_mac "$mac")"
        [[ -n "$card" ]] && {
            printf '%s' "$card"
            return 0
        }
        sleep 0.4
    done

    return 1
}

bt_audio_wait_for_sink() {
    local mac="$1"
    local sink=""
    local _i

    for _i in 1 2 3 4 5; do
        sink="$(bt_audio_sink_for_mac "$mac")"
        [[ -n "$sink" ]] && {
            printf '%s' "$sink"
            return 0
        }
        sleep 0.3
    done

    return 1
}

bt_audio_set_default_output() {
    local mac="$1"
    local sink

    sink="$(bt_audio_sink_for_mac "$mac")"
    [[ -n "$sink" ]] || return 1

    pactl set-default-sink "$sink" || return 1
    pactl list sink-inputs short 2>/dev/null | awk '{print $1}' |
        while read -r input_id; do
            [[ -n "$input_id" ]] && pactl move-sink-input "$input_id" "$sink" >/dev/null 2>&1 || true
        done
}

bt_audio_set_default_input() {
    local mac="$1"
    local source

    source="$(bt_audio_source_for_mac "$mac")"
    [[ -n "$source" ]] || return 1

    pactl set-default-source "$source" || return 1
}

bt_audio_use() {
    local mac="$1"
    local mode="${2:-a2dp}"
    local card profile

    bluetoothctl power on >/dev/null 2>&1 || true
    bluetoothctl trust "$mac" >/dev/null 2>&1 || true

    if ! bluetoothctl info "$mac" 2>/dev/null | grep -q 'Connected: yes'; then
        bluetoothctl connect "$mac" >/dev/null 2>&1 || return 1
    fi

    card="$(bt_audio_wait_for_card "$mac")" || return 1
    profile="$(bt_audio_best_profile "$card" "$mode")" || return 1
    pactl set-card-profile "$card" "$profile" >/dev/null 2>&1 || return 1

    bt_audio_wait_for_sink "$mac" >/dev/null 2>&1 || true
    bt_audio_set_default_output "$mac" || return 1

    if [[ "$mode" == "headset" ]]; then
        bt_audio_set_default_input "$mac" || true
    fi
}

bt_audio_rows() {
    {
        bluetoothctl devices Paired 2>/dev/null
        bluetoothctl devices Connected 2>/dev/null
        bluetoothctl devices 2>/dev/null
    } | awk '
        /^Device/ {
            mac = $2
            name = $0
            sub(/^Device [^ ]+ /, "", name)
            if (!seen[mac]++ && name != "") {
                print mac "\t" name
            }
        }
    ' | while IFS=$'\t' read -r mac name; do
        local info connected paired trusted battery card sink source profile
        info="$(bluetoothctl info "$mac" 2>/dev/null || true)"
        bt_audio_info_is_audio "$info" || continue

        connected="$(printf '%s\n' "$info" | awk -F': ' '/Connected:/ {print $2; exit}')"
        paired="$(printf '%s\n' "$info" | awk -F': ' '/Paired:/ {print $2; exit}')"
        trusted="$(printf '%s\n' "$info" | awk -F': ' '/Trusted:/ {print $2; exit}')"
        battery="$(bt_audio_battery_from_info "$info")"
        card="$(bt_audio_card_for_mac "$mac")"
        sink="$(bt_audio_sink_for_mac "$mac")"
        source="$(bt_audio_source_for_mac "$mac")"
        profile="$(bt_audio_active_profile "$card")"

        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "$name" "$mac" "${connected:-no}" "${paired:-no}" "${trusted:-no}" \
            "$battery" "$card" "$sink" "$source" "$profile"
    done
}

bt_audio_connected_summary() {
    bt_audio_rows | awk -F'\t' '$3 == "yes" {
        suffix = ($6 != "") ? " " $6 "%" : ""
        print $1 suffix
        exit
    }'
}
