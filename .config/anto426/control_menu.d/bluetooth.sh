#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

bluetooth_powered() {
    bluetoothctl show 2>/dev/null | awk -F': ' '/Powered:/ {print $2; exit}'
}

bluetooth_toggle() {
    local powered
    powered="$(bluetooth_powered)"

    if [[ "$powered" == "yes" ]]; then
        run_or_notify "Bluetooth spento" bluetoothctl power off
    else
        rfkill unblock bluetooth 2>/dev/null || true
        run_or_notify "Bluetooth acceso" bluetoothctl power on
    fi
}

bluetooth_scan() {
    [[ "$(bluetooth_powered)" == "yes" ]] || {
        notify "Accendi il Bluetooth prima della scansione"
        return 1
    }

    notify "Scansione Bluetooth avviata..."
    (
        timeout 8s bluetoothctl scan on >/dev/null 2>&1 || true
        bluetoothctl scan off >/dev/null 2>&1 || true
        notify "Scansione Bluetooth completata"
    ) &
}

bluetooth_device_rows() {
    {
        bluetoothctl devices 2>/dev/null
        bluetoothctl devices Paired 2>/dev/null
        bluetoothctl devices Connected 2>/dev/null
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
        local info connected paired trusted status
        info="$(bluetoothctl info "$mac" 2>/dev/null || true)"
        connected="$(printf '%s\n' "$info" | awk -F': ' '/Connected:/ {print $2; exit}')"
        paired="$(printf '%s\n' "$info" | awk -F': ' '/Paired:/ {print $2; exit}')"
        trusted="$(printf '%s\n' "$info" | awk -F': ' '/Trusted:/ {print $2; exit}')"

        status="disponibile"
        if [[ "$paired" == "yes" ]]; then
            status="associato"
        fi
        [[ "$trusted" == "yes" ]] && status="$status, attendibile"
        if [[ "$connected" == "yes" ]]; then
            status="connesso ($status)"
        fi

        printf '%s\t%s\t%s\t%s\n' "$name" "$mac" "$status" "$connected"
    done
}

bluetooth_device_menu() {
    local item="$1"
    local name mac status choice connected

    name="$(printf '%s' "$item" | awk -F'\t' '{print $1}')"
    mac="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    status="$(printf '%s' "$item" | awk -F'\t' '{print $3}')"
    connected="$(printf '%s' "$item" | awk -F'\t' '{print $4}')"

    while true; do
        local conn_label pair_label trust_label info trusted paired status_line
        info="$(bluetoothctl info "$mac" 2>/dev/null || true)"
        connected="$(printf '%s\n' "$info" | awk -F': ' '/Connected:/ {print $2; exit}')"
        paired="$(printf '%s\n' "$info" | awk -F': ' '/Paired:/ {print $2; exit}')"
        trusted="$(printf '%s\n' "$info" | awk -F': ' '/Trusted:/ {print $2; exit}')"

        if [[ "$connected" == "yes" ]]; then
            conn_label="$(system_text "Disconnect")"
        else
            conn_label="$(system_text "Connect")"
        fi

        if [[ "$paired" == "yes" ]]; then
            pair_label="$(system_text "Unpair")"
        else
            pair_label="$(system_text "Pair")"
        fi

        if [[ "$trusted" == "yes" ]]; then
            trust_label="$(system_text "Untrust")"
        else
            trust_label="$(system_text "Trust")"
        fi

        status_line="Connected: ${connected:-no}\nPaired: ${paired:-no}\nTrusted: ${trusted:-no}"

        choice="$(
            {
                printf '%s\0icon\x1fnetwork-wireless\n' "$conn_label"
                printf '%s\0icon\x1fpreferences-system-bluetooth\n' "$pair_label"
                printf '%s\0icon\x1fsecurity-high\n' "$trust_label"
                printf '%s\0icon\x1fedit-copy\n' "$(system_text "Copy MAC")"
                printf '%s\0icon\x1fuser-trash\n' "$(system_text "Remove device")"
                printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
            } | rofi_pick_msg "Device: $name" "MAC: $mac\n${status_line}"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Disconnect")")
                run_or_notify "Disconnesso da $name" bluetoothctl disconnect "$mac"
                ;;
            "$(system_text "Connect")")
                run_or_notify "Connesso a $name" bluetoothctl connect "$mac"
                ;;
            "$(system_text "Unpair")")
                run_or_notify "Associazione rimossa per $name" bluetoothctl remove "$mac"
                ;;
            "$(system_text "Pair")")
                run_or_notify "Associato a $name" bluetoothctl pair "$mac"
                ;;
            "$(system_text "Untrust")")
                run_or_notify "$name rimosso da attendibili" bluetoothctl untrust "$mac"
                ;;
            "$(system_text "Trust")")
                run_or_notify "$name contrassegnato attendibile" bluetoothctl trust "$mac"
                ;;
            "$(system_text "Copy MAC")")
                if command -v wl-copy >/dev/null 2>&1; then
                    printf '%s' "$mac" | wl-copy && notify "MAC copiato"
                else
                    notify "wl-copy non disponibile"
                fi
                ;;
            "$(system_text "Remove device")")
                run_or_notify "Dispositivo $name rimosso" bluetoothctl remove "$mac"
                return 0
                ;;
            *)
                if [[ "$choice" == "$(system_text "Back")" ]]; then
                    return 0
                fi
                ;;
        esac
    done
}

bluetooth_menu() {
    while true; do
        local powered state choice device_rows connected_bt
        powered="$(bluetooth_powered)"
        state="off"
        [[ "$powered" == "yes" ]] && state="on"
        [[ -z "$powered" ]] && state="not available"

        device_rows="$(bluetooth_device_rows)"
        connected_bt="$(printf '%s\n' "$device_rows" | awk -F'\t' '$4 == "yes" {print $1; exit}')"
        [[ -z "$connected_bt" ]] && connected_bt="none"

        local state_color="$c_red"
        [[ "$state" == "on" ]] && state_color="$c_green"
        local conn_color="$c_muted"
        [[ "$connected_bt" != "none" ]] && conn_color="$c_yellow"
        local message_card
        message_card="Status: <b><span foreground='${state_color}'>Bluetooth ${state}</span></b>\nConnected: <b><span foreground='${conn_color}'>${connected_bt}</span></b>"

        choice="$(
            {
                printf '%s\0icon\x1fpreferences-system-bluetooth\n' "$(menu_item "󰂯" "Enable/Disable")"
                printf '%s\0icon\x1fsystem-search\n' "$(menu_item "󰑐" "Scan devices")"
                if command -v blueman-manager >/dev/null 2>&1; then
                    printf '%s\0icon\x1fpreferences-system\n' "$(menu_item "󰂯" "Bluetooth settings")"
                fi
                if [[ -n "$device_rows" ]]; then
                    local dev_line
                    while IFS= read -r dev_line; do
                        [[ -z "$dev_line" ]] && continue
                        local name mac status connected
                        name="$(printf '%s' "$dev_line" | awk -F'\t' '{print $1}')"
                        mac="$(printf '%s' "$dev_line" | awk -F'\t' '{print $2}')"
                        status="$(printf '%s' "$dev_line" | awk -F'\t' '{print $3}')"
                        connected="$(printf '%s' "$dev_line" | awk -F'\t' '{print $4}')"
                        local icon="bluetooth"
                        if [[ "$connected" == "yes" ]]; then
                            icon="bluetooth-active"
                        fi
                        printf '%s  (%s)\0icon\x1f%s\n' "$name" "$status" "$icon"
                    done <<< "$device_rows"
                fi
                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "Bluetooth" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Enable/Disable")")
                bluetooth_toggle
                ;;
            "$(system_text "Scan devices")")
                bluetooth_scan
                ;;
            "$(system_text "Bluetooth settings")")
                open_or_notify "Bluetooth settings" blueman-manager
                return 0
                ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
            *)
                local chosen_name
                chosen_name="$(printf '%s' "$choice" | sed -E 's/[[:space:]]+\([^)]+\)$//')"
                local dev_item
                dev_item="$(printf '%s\n' "$device_rows" | awk -F'\t' -v name="$chosen_name" '$1 == name {printf "%s\t%s\t%s\t%s", $1, $2, $3, $4; exit}')"
                [[ -n "$dev_item" ]] && bluetooth_device_menu "$dev_item"
                ;;
        esac
    done
}

bluetooth_menu

