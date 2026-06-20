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
        local info connected paired trusted marker status
        info="$(bluetoothctl info "$mac" 2>/dev/null || true)"
        connected="$(printf '%s\n' "$info" | awk -F': ' '/Connected:/ {print $2; exit}')"
        paired="$(printf '%s\n' "$info" | awk -F': ' '/Paired:/ {print $2; exit}')"
        trusted="$(printf '%s\n' "$info" | awk -F': ' '/Trusted:/ {print $2; exit}')"

        marker="󰂲"
        status="disponibile"
        if [[ "$paired" == "yes" ]]; then
            marker="󰂯"
            status="associato"
        fi
        [[ "$trusted" == "yes" ]] && status="$status, attendibile"
        if [[ "$connected" == "yes" ]]; then
            marker="󰂱"
            status="connesso ($status)"
        fi

        printf '%s  %s\t%s\t%s\t%s\t%s\n' "$marker" "$name" "$mac" "$status" "$connected" "$name"
    done
}

bluetooth_device_menu() {
    local item="$1"
    local label mac status choice connected name

    label="$(printf '%s' "$item" | awk -F'\t' '{print $1}')"
    mac="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    status="$(printf '%s' "$item" | awk -F'\t' '{print $3}')"
    connected="$(printf '%s' "$item" | awk -F'\t' '{print $4}')"
    name="$(printf '%s' "$item" | awk -F'\t' '{print $5}')"

    while true; do
        local conn_label pair_label trust_label info trusted paired status_line
        info="$(bluetoothctl info "$mac" 2>/dev/null || true)"
        connected="$(printf '%s\n' "$info" | awk -F': ' '/Connected:/ {print $2; exit}')"
        paired="$(printf '%s\n' "$info" | awk -F': ' '/Paired:/ {print $2; exit}')"
        trusted="$(printf '%s\n' "$info" | awk -F': ' '/Trusted:/ {print $2; exit}')"

        conn_label="Connect"
        [[ "$connected" == "yes" ]] && conn_label="Disconnect"
        pair_label="Pair"
        [[ "$paired" == "yes" ]] && pair_label="Unpair"
        trust_label="Trust"
        [[ "$trusted" == "yes" ]] && trust_label="Untrust"
        status_line="Connected: ${connected:-no}\nPaired: ${paired:-no}\nTrusted: ${trusted:-no}"

        choice="$(
            {
                printf '%s\0icon\x1fnetwork-wireless\n' "$conn_label"
                printf '%s\0icon\x1fpreferences-system-bluetooth\n' "$pair_label"
                printf '%s\0icon\x1fsecurity-high\n' "$trust_label"
                printf 'Copy MAC\0icon\x1fedit-copy\n'
                printf 'Remove device\0icon\x1fuser-trash\n'
                printf 'Back\0icon\x1fgo-previous\n'
            } | rofi_pick_msg "Device: $name" "MAC: $mac\n${status_line}"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            *Connect*)
                run_or_notify "Connesso a $name" bluetoothctl connect "$mac"
                ;;
            *Disconnect*)
                run_or_notify "Disconnesso da $name" bluetoothctl disconnect "$mac"
                ;;
            *Pair*)
                run_or_notify "Associato a $name" bluetoothctl pair "$mac"
                ;;
            *Unpair*)
                run_or_notify "Associazione rimossa per $name" bluetoothctl remove "$mac"
                ;;
            *Trust*)
                run_or_notify "$name contrassegnato attendibile" bluetoothctl trust "$mac"
                ;;
            *Untrust*)
                run_or_notify "$name rimosso da attendibili" bluetoothctl untrust "$mac"
                ;;
            "Copy MAC"*)
                if command -v wl-copy >/dev/null 2>&1; then
                    printf '%s' "$mac" | wl-copy && notify "MAC copiato"
                else
                    notify "wl-copy non disponibile"
                fi
                ;;
            "Remove device"*)
                run_or_notify "Dispositivo $name rimosso" bluetoothctl remove "$mac"
                return 0
                ;;
            "Back"*)
                return 0
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
        connected_bt="$(printf '%s\n' "$device_rows" | awk -F'\t' '$4 == "yes" {print $5; exit}')"
        [[ -z "$connected_bt" ]] && connected_bt="none"

        local state_color="$c_red"
        [[ "$state" == "on" ]] && state_color="$c_green"
        local conn_color="$c_muted"
        [[ "$connected_bt" != "none" ]] && conn_color="$c_yellow"
        local message_card
        message_card="Status: <b><span foreground='${state_color}'>Bluetooth ${state}</span></b>\nConnected: <b><span foreground='${conn_color}'>${connected_bt}</span></b>"

        choice="$(
            {
                printf 'Enable/Disable\0icon\x1fpreferences-system-bluetooth\n'
                printf 'Scan devices\0icon\x1fsystem-search\n'
                if command -v blueman-manager >/dev/null 2>&1; then
                    printf 'Bluetooth settings\0icon\x1fpreferences-system\n'
                fi
                if [[ -n "$device_rows" ]]; then
                    local dev_line
                    while IFS= read -r dev_line; do
                        [[ -z "$dev_line" ]] && continue
                        local marker name mac status connected
                        marker="$(printf '%s' "$dev_line" | awk -F'\t' '{print $1}')"
                        name="$(printf '%s' "$dev_line" | awk -F'\t' '{print $6}')"
                        mac="$(printf '%s' "$dev_line" | awk -F'\t' '{print $2}')"
                        status="$(printf '%s' "$dev_line" | awk -F'\t' '{print $3}')"
                        local icon="bluetooth"
                        if [[ "$marker" == "󰂱" ]]; then
                            icon="bluetooth-active"
                        fi
                        printf '%s\t(%s)\0icon\x1f%s\n' "$name" "$status" "$icon"
                    done <<< "$device_rows"
                fi
                printf 'Back\0icon\x1fgo-previous\n'
            } | rofi_pick_msg "Bluetooth" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "Enable/Disable"*)
                bluetooth_toggle
                ;;
            "Scan devices"*)
                bluetooth_scan
                ;;
            "Bluetooth settings"*)
                open_or_notify "Bluetooth settings" blueman-manager
                return 0
                ;;
            "Back"*)
                back_or_main
                return 0
                ;;
            *)
                local chosen_name
                chosen_name="$(printf '%s' "$choice" | awk -F'\t' '{print $1}')"
                local dev_item
                dev_item="$(printf '%s\n' "$device_rows" | awk -F'\t' -v name="$chosen_name" '$6 == name {printf "%s\t%s\t%s\t%s\t%s", $1, $2, $3, $4, $5; exit}')"
                [[ -n "$dev_item" ]] && bluetooth_device_menu "$dev_item"
                ;;
        esac
    done
}

bluetooth_menu
