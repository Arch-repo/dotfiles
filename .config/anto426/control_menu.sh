#!/usr/bin/env bash
set -uo pipefail

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/anto426"
EVENTS_FILE="$DATA_DIR/calendar/events.json"
GCAL_EVENTS_FILE="$DATA_DIR/calendar/google_events.json"
NOTIFICATIONS_FILE="$DATA_DIR/notifications/history.tsv"
WIFI_CACHE_DIR="$DATA_DIR/wifi"
WIFI_CACHE_FILE="$WIFI_CACHE_DIR/networks.tsv"
WIFI_CACHE_MAX_AGE=90
THEME_MENU="$HOME/.config/rofi/control_menu.rasi"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=rofi_slider.sh
source "$SCRIPT_DIR/rofi_slider.sh"
THEME_CALENDAR="$HOME/.config/rofi/control_calendar.rasi"
REMOTE_SYNC_SCRIPT="$HOME/.config/anto426/remote_sync.sh"

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
    printf '%s  %s' "$icon" "$(system_text "$msgid")"
}

rofi_pick() {
    local prompt="$1"
    rofi -dmenu -i -matching fuzzy -p "$prompt" -theme "$THEME_MENU"
}

rofi_pick_msg() {
    local prompt="$1"
    local message="$2"
    local theme="${3:-$THEME_MENU}"

    # Expand any literal backslash escapes (like \n) into actual formatting newlines
    message="$(printf '%b' "$message")"

    rofi -dmenu -i -matching fuzzy -p "$prompt" -mesg "$message" -theme "$theme"
}

rofi_input() {
    local prompt="$1"
    local value="${2:-}"
    printf '%s' "$value" | rofi -dmenu -p "$prompt" -theme "$THEME_MENU"
}

rofi_password() {
    local prompt="$1"
    local message="${2:-}"

    if [[ -n "$message" ]]; then
        rofi -dmenu -password -p "$prompt" -mesg "$message" -theme "$THEME_MENU"
    else
        rofi -dmenu -password -p "$prompt" -theme "$THEME_MENU"
    fi
}

notify() {
    notify-send "Menu" "$*" 2>/dev/null || true
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

# Finite State Machine Control Variable
MENU_STATE="${1:-main}"

back_or_main() {
    MENU_STATE="main"
}

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
        timeout 7s bluetoothctl scan on >/dev/null 2>&1 || true
    ) &
}

bluetooth_device_rows() {
    {
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
    local label mac status choice

    label="$(printf '%s' "$item" | awk -F'\t' '{print $1}')"
    mac="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    status="$(printf '%s' "$item" | awk -F'\t' '{print $3}')"
    [[ -n "$mac" ]] || return 0

    while true; do
        choice="$(
            printf '%s\n' \
                "Status: $status" \
                "󰌷  Connect" \
                "󰌸  Disconnect" \
                "󰖔  Pair" \
                "󰗠  Trust" \
                "󰆴  Remove" \
                "󰋼  Info" \
                "" \
                "󰌍  Back" |
                rofi_pick "$label"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰌷  Connect")
                notify "Connecting to $label..."
                run_or_notify "Bluetooth Connection" bluetoothctl connect "$mac"
                return 0
                ;;
            "󰌸  Disconnect")
                notify "Disconnecting from $label..."
                run_or_notify "Bluetooth Disconnection" bluetoothctl disconnect "$mac"
                return 0
                ;;
            "󰖔  Pair")
                run_or_notify "Bluetooth Pairing" bluetoothctl pair "$mac"
                ;;
            "󰗠  Trust")
                run_or_notify "Bluetooth Trust" bluetoothctl trust "$mac"
                ;;
            "󰆴  Remove")
                run_or_notify "Device removed" bluetoothctl remove "$mac"
                return 0
                ;;
            "󰋼  Info")
                bluetoothctl info "$mac" 2>&1 | rofi_pick "Bluetooth Info" >/dev/null
                ;;
            "󰌍  Back")
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}

bluetooth_menu() {
    while true; do
        local powered state choice device_rows devices connected_bt
        powered="$(bluetooth_powered)"
        state="off"
        [[ "$powered" == "yes" ]] && state="on"
        [[ -z "$powered" ]] && state="not available"

        device_rows="$(bluetooth_device_rows)"
        connected_bt="$(printf '%s\n' "$device_rows" | awk -F'\t' '$4 == "yes" {print $5; exit}')"
        [[ -z "$connected_bt" ]] && connected_bt="none"

        # Format devices with tree-like connectors
        local formatted_devices=""
        if [[ -n "$device_rows" ]]; then
            local count dev_lines i
            mapfile -t dev_lines <<< "$(printf '%s\n' "$device_rows" | awk -F'\t' '{print $1}')"
            count=${#dev_lines[@]}
            if [[ $count -gt 0 && -n "${dev_lines[0]}" ]]; then
                for ((i=0; i<count; i++)); do
                    if (( i == count - 1 )); then
                        formatted_devices+=$' └─ '"${dev_lines[i]}"$'\n'
                    else
                        formatted_devices+=$' ├─ '"${dev_lines[i]}"$'\n'
                    fi
                done
            else
                formatted_devices=" └─ 󰂲  No devices found\n"
            fi
        else
            formatted_devices=" └─ 󰂲  No devices found\n"
        fi

        local state_color="$c_red"
        [[ "$state" == "on" ]] && state_color="$c_green"
        local conn_color="$c_muted"
        [[ "$connected_bt" != "none" ]] && conn_color="$c_yellow"
        local message_card
        message_card="Status: <b><span foreground='${state_color}'>Bluetooth ${state}</span></b>\nConnected: <b><span foreground='${conn_color}'>${connected_bt}</span></b>"

        choice="$(
            {
                printf '󰂯 BLUETOOTH MANAGEMENT\n'
                printf ' ├─ 󰐕  Enable/Disable\n'
                printf ' └─ 󰑓  Scan devices\n'
                
                printf '\n󰂱 DEVICES\n'
                printf '%s' "$formatted_devices"
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "Bluetooth" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "󰐕 Enable/Disable" | "󰐕  Enable/Disable")
                bluetooth_toggle
                ;;
            "󰑓 Scan devices" | "󰑓  Scan devices")
                bluetooth_scan
                ;;
            "󰂲 No devices found" | "󰂲  No devices found")
                continue
                ;;
            *)
                local dev_item
                dev_item="$(printf '%s\n' "$device_rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {printf "%s\t%s\t%s", $1, $2, $3; exit}')"
                [[ -n "$dev_item" ]] && bluetooth_device_menu "$dev_item"
                ;;
        esac
    done
}

wifi_enabled() {
    timeout 1s nmcli radio wifi 2>/dev/null || true
}

wifi_connected_ssid() {
    timeout 1s nmcli -t -f TYPE,NAME connection show --active 2>/dev/null |
        awk -F: '$1 == "802-11-wireless" {print $2; exit}'
}

wifi_toggle() {
    if [[ "$(wifi_enabled)" == "enabled" ]]; then
        run_or_notify "Wi-Fi spento" nmcli radio wifi off
    else
        run_or_notify "Wi-Fi acceso" nmcli radio wifi on
    fi
}

wifi_network_rows() {
    awk -F: '
        $2 != "" {
            ssid = $2
            if (!seen[ssid]) {
                order[++n] = ssid
                seen[ssid] = 1
            }
            if (active_seen[ssid] && $1 != "yes") next

            sig = $4 + 0
            if (sig >= 80) marker = "󰤨"
            else if (sig >= 60) marker = "󰤥"
            else if (sig >= 40) marker = "󰤢"
            else if (sig >= 20) marker = "󰤟"
            else marker = "󰤯"

            if ($1 == "yes") marker = "󰤨"

            security = ($3 == "" || $3 == "--") ? "open" : $3
            lock = (security == "open") ? "" : "  󰌿"
            active_str = ($1 == "yes") ? " (Connesso)" : ""
            label = sprintf("%s  %s%s%s  %d%%", marker, $2, lock, active_str, sig)

            labels[ssid] = label
            securities[ssid] = security
            actives[ssid] = $1
            if ($1 == "yes") active_seen[ssid] = 1
        }
        END {
            for (i = 1; i <= n; i++) {
                ssid = order[i]
                printf "%s\t%s\t%s\t%s\n", labels[ssid], ssid, securities[ssid], actives[ssid]
            }
        }
    '
}

wifi_cache_age() {
    [[ -s "$WIFI_CACHE_FILE" ]] || return 1
    local now updated
    now="$(date +%s)"
    updated="$(stat -c %Y "$WIFI_CACHE_FILE" 2>/dev/null || printf '0')"
    printf '%s' "$((now - updated))"
}

wifi_cache_fresh() {
    local age
    age="$(wifi_cache_age 2>/dev/null)" || return 1
    (( age < WIFI_CACHE_MAX_AGE ))
}

wifi_cache_update() {
    mkdir -p "$WIFI_CACHE_DIR"

    local raw_list tmp
    tmp="$(mktemp)"

    if raw_list="$(timeout 8s nmcli -t -f ACTIVE,SSID,SECURITY,SIGNAL dev wifi list --rescan no 2>/dev/null)"; then
        printf '%s\n' "$raw_list" | wifi_network_rows > "$tmp"
        if [[ -s "$tmp" ]]; then
            mv "$tmp" "$WIFI_CACHE_FILE"
            return 0
        fi
    fi

    rm -f "$tmp"
    return 1
}

wifi_cache_refresh_background() {
    local force="${1:-false}"
    local lock_dir="${XDG_RUNTIME_DIR:-/tmp}/anto426-wifi-cache.lock"

    if [[ -d "$lock_dir" ]]; then
        local lock_age
        lock_age="$(($(date +%s) - $(stat -c %Y "$lock_dir" 2>/dev/null || date +%s)))"
        if (( lock_age > 15 )); then
            rmdir "$lock_dir" 2>/dev/null || rm -rf "$lock_dir" 2>/dev/null
        fi
    fi

    [[ "$force" == "true" ]] || ! wifi_cache_fresh || return 0
    mkdir "$lock_dir" 2>/dev/null || return 0

    (
        wifi_cache_update || true
        rmdir "$lock_dir" 2>/dev/null || true
    ) &
}

wifi_cached_rows() {
    if [[ -s "$WIFI_CACHE_FILE" ]]; then
        cat "$WIFI_CACHE_FILE"
        return 0
    fi

    local raw_list
    raw_list="$(timeout 4s nmcli -t -f ACTIVE,SSID,SECURITY,SIGNAL dev wifi list --rescan no 2>/dev/null || true)"
    printf '%s\n' "$raw_list" | wifi_network_rows
}

wifi_cache_status() {
    local age
    age="$(wifi_cache_age 2>/dev/null)" || {
        printf 'List: updating...'
        return 0
    }

    if (( age < 60 )); then
        printf 'List: updated %ss ago' "$age"
    else
        printf 'List: updated %sm ago' "$((age / 60))"
    fi
}

wifi_profile_name_for_ssid() {
    local ssid="$1"
    local name profile_ssid

    while IFS= read -r name; do
        [[ -n "$name" ]] || continue
        profile_ssid="$(nmcli -g 802-11-wireless.ssid connection show "$name" 2>/dev/null | sed -n '1p')"
        if [[ "$profile_ssid" == "$ssid" || "$name" == "$ssid" ]]; then
            printf '%s' "$name"
            return 0
        fi
    done < <(nmcli -g NAME connection show 2>/dev/null)

    return 1
}

wifi_profile_exists() {
    wifi_profile_name_for_ssid "$1" >/dev/null
}

wifi_security_kind() {
    local security="${1:-open}"
    security="$(printf '%s' "$security" | tr '[:lower:]' '[:upper:]')"

    if [[ -z "$security" || "$security" == "--" || "$security" == "OPEN" ]]; then
        printf 'open'
    elif [[ "$security" == *"802.1X"* ]]; then
        printf 'enterprise'
    elif [[ "$security" == *"WEP"* ]]; then
        printf 'wep'
    elif [[ "$security" == *"WPA"* || "$security" == *"SAE"* ]]; then
        printf 'wpa'
    else
        printf 'secured'
    fi
}

wifi_password_hint() {
    case "$(wifi_security_kind "$1")" in
        wpa) printf 'Password WPA/WPA2/WPA3: 8-63 caratteri, oppure chiave hex da 64 caratteri.' ;;
        wep) printf 'Chiave WEP: 5 o 13 caratteri, oppure 10 o 26 caratteri esadecimali.' ;;
        *) printf 'Inserisci la password della rete. Vuoto = annulla.' ;;
    esac
}

wifi_password_valid() {
    local security="$1"
    local password="$2"
    local length
    length="${#password}"

    case "$(wifi_security_kind "$security")" in
        open) return 0 ;;
        enterprise) return 1 ;;
        wpa)
            ((length >= 8 && length <= 63)) && return 0
            [[ "$password" =~ ^[0-9A-Fa-f]{64}$ ]] && return 0
            return 1
            ;;
        wep)
            case "$length" in
                5 | 13) return 0 ;;
                10 | 26) [[ "$password" =~ ^[0-9A-Fa-f]+$ ]] && return 0 ;;
            esac
            return 1
            ;;
        *)
            [[ -n "$password" ]]
            ;;
    esac
}

wifi_output_needs_password() {
    local output="$1"
    printf '%s' "$output" | grep -Eiq 'secrets?|password|passphrase|key|802-11-wireless-security|No agents were available|authentication|not authorized'
}

wifi_rescan() {
    notify "Aggiorno reti Wi-Fi in background..."
    (
        nmcli dev wifi rescan >/dev/null 2>&1 || true
        wifi_cache_update >/dev/null 2>&1 || true
        notify "Reti Wi-Fi aggiornate"
    ) &
}

wifi_connect_new() {
    local ssid="$1"
    local security="$2"
    local password output attempt hint

    if [[ "$(wifi_security_kind "$security")" == "enterprise" ]]; then
        notify "Rete 802.1X: serve un profilo già configurato"
        return 1
    fi

    if [[ "$(wifi_security_kind "$security")" == "open" ]]; then
        notify "Connessione in corso a $ssid..."
        if output="$(nmcli -w 20 dev wifi connect "$ssid" 2>&1)"; then
            notify "Connesso a $ssid"
            wifi_cache_update >/dev/null 2>&1 || true
        else
            if wifi_output_needs_password "$output"; then
                notify "$ssid sembra richiedere una password"
                wifi_connect_new "$ssid" "WPA"
                return $?
            else
                notify "Connessione a $ssid fallita: $output"
            fi
        fi
        return 0
    fi

    hint="$(wifi_password_hint "$security")"
    for attempt in 1 2 3; do
        if ! password="$(rofi_password "Password Wi-Fi ($ssid)" "$hint")"; then
            notify "Connessione a $ssid annullata"
            return 1
        fi

        if [[ -z "$password" ]]; then
            notify "Password non inserita: connessione annullata"
            return 1
        fi

        if ! wifi_password_valid "$security" "$password"; then
            hint="$(wifi_password_hint "$security")"
            notify "Password non valida per $ssid"
            continue
        fi

        notify "Connessione in corso a $ssid..."
        if output="$(nmcli -w 30 dev wifi connect "$ssid" password "$password" 2>&1)"; then
            notify "Connesso a $ssid"
            wifi_cache_update >/dev/null 2>&1 || true
            return 0
        fi

        if wifi_output_needs_password "$output"; then
            hint="Password non accettata. Riprova.\n$(wifi_password_hint "$security")"
            notify "Password non accettata per $ssid"
            continue
        fi

        notify "Connessione a $ssid fallita: $output"
        return 1
    done

    notify "Connessione a $ssid non riuscita dopo 3 tentativi"
    return 1
}

wifi_connect_saved() {
    local ssid="$1"
    local security="$2"
    local profile_name="${3:-$1}"
    local output

    notify "Connessione in corso a $ssid..."
    if output="$(nmcli -w 20 connection up id "$profile_name" 2>&1)"; then
        notify "Connesso a $ssid"
        wifi_cache_update >/dev/null 2>&1 || true
        return 0
    fi

    if wifi_output_needs_password "$output"; then
        notify "Profilo salvato senza password valida"
        wifi_connect_new "$ssid" "$security"
        return $?
    fi

    notify "Connessione a $ssid fallita: $output"
    return 1
}

wifi_device_menu() {
    local item="$1"
    local label ssid security choice active has_profile profile_name

    label="$(printf '%s' "$item" | awk -F'\t' '{print $1}')"
    ssid="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    security="$(printf '%s' "$item" | awk -F'\t' '{print $3}')"
    active="$(printf '%s' "$item" | awk -F'\t' '{print $4}')"
    [[ -n "$ssid" ]] || return 0

    profile_name="$(wifi_profile_name_for_ssid "$ssid" 2>/dev/null || true)"
    if [[ -n "$profile_name" ]]; then
        has_profile=true
    else
        has_profile=false
    fi

    while true; do
        local options=()
        if [[ "$active" == "yes" ]]; then
            options+=("󰌸  Disconnect")
        else
            options+=("󰌷  Connect")
        fi

        if [[ "$has_profile" == "true" ]]; then
            options+=("󰆴  Forget network")
        fi

        options+=("󰋼  Info")
        options+=("")
        options+=("󰌍  Back")

        local options_str
        options_str="$(printf '%s\n' "${options[@]}")"

        choice="$(printf '%s\n' "$options_str" | rofi_pick "$ssid")"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰌷  Connetti" | "󰌷  Connect")
                if [[ "$has_profile" == "true" ]]; then
                    wifi_connect_saved "$ssid" "$security" "$profile_name"
                else
                    wifi_connect_new "$ssid" "$security"
                fi
                return 0
                ;;
            "󰌸  Disconnetti" | "󰌸  Disconnect")
                notify "Disconnecting from $ssid..."
                run_or_notify "Disconnection from $ssid" nmcli connection down id "${profile_name:-$ssid}"
                wifi_cache_update >/dev/null 2>&1 || true
                return 0
                ;;
            "󰆴  Dimentica rete" | "󰆴  Forget network")
                run_or_notify "Network forgotten" nmcli connection delete id "${profile_name:-$ssid}"
                wifi_cache_update >/dev/null 2>&1 || true
                return 0
                ;;
            "󰋼  Informazioni" | "󰋼  Info")
                if [[ "$has_profile" == "true" ]]; then
                    local raw_info uuid device state ip4 gw4 dns4 ip6 gw6 dns6
                    raw_info="$(nmcli connection show id "$profile_name" 2>/dev/null)"
                    
                    ssid="$(printf '%s\n' "$raw_info" | awk '/^(connection.id|GENERAL.NAME):/ {sub(/^(connection.id|GENERAL.NAME):[ \t]*/, ""); print; exit}')"
                    [[ -z "$ssid" ]] && ssid="$profile_name"
                    
                    uuid="$(printf '%s\n' "$raw_info" | awk '/^(connection.uuid|GENERAL.UUID):/ {sub(/^(connection.uuid|GENERAL.UUID):[ \t]*/, ""); print; exit}')"
                    device="$(printf '%s\n' "$raw_info" | awk '/^(connection.interface-name|GENERAL.DEVICES):/ {sub(/^(connection.interface-name|GENERAL.DEVICES):[ \t]*/, ""); print; exit}')"
                    [[ -z "$device" || "$device" == "--" ]] && device="$(printf '%s\n' "$raw_info" | awk '/^GENERAL.IP-IFACE:/ {sub(/^GENERAL.IP-IFACE:[ \t]*/, ""); print; exit}')"
                    state="$(printf '%s\n' "$raw_info" | awk '/^GENERAL.STATE:/ {sub(/^GENERAL.STATE:[ \t]*/, ""); print; exit}')"
                    [[ -z "$state" ]] && state="inactive"

                    ip4="$(printf '%s\n' "$raw_info" | awk '/^IP4.ADDRESS/ {sub(/^IP4.ADDRESS[^:]*:[ \t]*/, ""); print; exit}')"
                    gw4="$(printf '%s\n' "$raw_info" | awk '/^IP4.GATEWAY:/ {sub(/^IP4.GATEWAY:[ \t]*/, ""); print; exit}')"
                    dns4="$(printf '%s\n' "$raw_info" | awk '/^IP4.DNS/ {sub(/^IP4.DNS[^:]*:[ \t]*/, ""); print}' | paste -sd, - | sed 's/,/, /g')"
                    
                    ip6="$(printf '%s\n' "$raw_info" | awk '/^IP6.ADDRESS/ {sub(/^IP6.ADDRESS[^:]*:[ \t]*/, ""); print; exit}')"
                    gw6="$(printf '%s\n' "$raw_info" | awk '/^IP6.GATEWAY:/ {sub(/^IP6.GATEWAY:[ \t]*/, ""); print; exit}')"
                    dns6="$(printf '%s\n' "$raw_info" | awk '/^IP6.DNS/ {sub(/^IP6.DNS[^:]*:[ \t]*/, ""); print}' | paste -sd, - | sed 's/,/, /g')"

                    while true; do
                        local info_choice
                        info_choice="$(
                            {
                                printf '󰤨  SSID: %s\n' "${ssid}"
                                printf '󰡝  UUID: %s\n' "${uuid:---}"
                                printf '󰈀  Device: %s\n' "${device:---}"
                                printf '󰻠  State: %s\n' "${state}"
                                
                                if [[ -n "$ip4" && "$ip4" != "--" ]]; then
                                    printf '󰩠  IPv4: %s\n' "${ip4}"
                                    [[ -n "$gw4" && "$gw4" != "--" ]] && printf '󰟩  IPv4 Gateway: %s\n' "${gw4}"
                                    [[ -n "$dns4" && "$dns4" != "--" ]] && printf '󰗢  IPv4 DNS: %s\n' "${dns4}"
                                fi
                                
                                if [[ -n "$ip6" && "$ip6" != "--" ]]; then
                                    printf '󰩠  IPv6: %s\n' "${ip6}"
                                    [[ -n "$gw6" && "$gw6" != "--" ]] && printf '󰟩  IPv6 Gateway: %s\n' "${gw6}"
                                    [[ -n "$dns6" && "$dns6" != "--" ]] && printf '󰗢  IPv6 DNS: %s\n' "${dns6}"
                                fi
                                printf '\n󰌍  Back\n'
                            } | rofi -dmenu -i -p "Network Info" -theme "$THEME_MENU"
                        )"
                        
                        [[ -z "$info_choice" ]] && break
                        if [[ "$info_choice" == *"Back"* ]]; then
                            break
                        fi
                    done
                else
                    while true; do
                        local info_choice
                        info_choice="$(
                            {
                                printf '󰤨  SSID: %s\n' "${ssid}"
                                printf '󰻠  Status: Unsaved / Available\n'
                                printf '󰌿  Security: %s\n' "${security:---}"
                                printf '\n󰌍  Back\n'
                            } | rofi -dmenu -i -p "Network Info" -theme "$THEME_MENU"
                        )"
                        [[ -z "$info_choice" ]] && break
                        if [[ "$info_choice" == *"Back"* ]]; then
                            break
                        fi
                    done
                fi
                ;;
            "󰌍  Indietro" | "󰌍  Back")
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}

wifi_menu() {
    wifi_cache_refresh_background

    while true; do
        local enabled connected choice network_rows networks state cache_status
        enabled="$(wifi_enabled)"
        state="off"
        [[ "$enabled" == "enabled" ]] && state="on"
        [[ -z "$enabled" ]] && state="not available"

        connected="$(wifi_connected_ssid)"
        [[ -z "$connected" ]] && connected="no network"

        network_rows="$(wifi_cached_rows)"
        cache_status="$(wifi_cache_status)"

        local formatted_networks=""
        if [[ -n "$network_rows" ]]; then
            local count net_lines i
            mapfile -t net_lines <<< "$(printf '%s\n' "$network_rows" | awk -F'\t' '{print $1}')"
            count=${#net_lines[@]}
            if [[ $count -gt 0 && -n "${net_lines[0]}" ]]; then
                for ((i=0; i<count; i++)); do
                    if (( i == count - 1 )); then
                        formatted_networks+=$' └─ '"${net_lines[i]}"$'\n'
                    else
                        formatted_networks+=$' ├─ '"${net_lines[i]}"$'\n'
                    fi
                done
            else
                formatted_networks=" └─ 󰤭  Loading networks...\n"
            fi
        else
            formatted_networks=" └─ 󰤭  Loading networks...\n"
        fi

        local state_color="$c_red"
        [[ "$state" == "on" ]] && state_color="$c_green"
        local conn_color="$c_muted"
        [[ "$connected" != "no network" ]] && conn_color="$c_cyan"
        local message_card
        message_card="Status: <b><span foreground='${state_color}'>Wi-Fi ${state}</span></b>\nConnected: <b><span foreground='${conn_color}'>${connected}</span></b>\n<span foreground='${c_muted}'>${cache_status}</span>"

        choice="$(
            {
                printf '󰤨 WI-FI MANAGEMENT\n'
                printf ' ├─ 󰐕  Enable/Disable\n'
                printf ' └─ 󰑓  Scan networks\n'
                
                printf '\n󰤨 AVAILABLE NETWORKS\n'
                printf '%s' "$formatted_networks"
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "Wi-Fi" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "󰐕 Enable/Disable" | "󰐕  Enable/Disable")
                wifi_toggle
                wifi_cache_refresh_background true
                ;;
            "󰑓 Scan networks" | "󰑓  Scan networks")
                wifi_rescan
                ;;
            "󰤭 Loading networks..." | "󰤭  Loading networks...")
                wifi_cache_refresh_background true
                continue
                ;;
            *)
                local dev_item
                dev_item="$(printf '%s\n' "$network_rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {printf "%s\t%s\t%s\t%s", $1, $2, $3, $4; exit}')"
                [[ -n "$dev_item" ]] && wifi_device_menu "$dev_item"
                ;;
        esac
    done
}

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
                marker = (name == def) ? "*" : " "
                printf "%s %s\t%s\n", marker, $0, name
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
                marker = (name == def) ? "*" : " "
                printf "%s %s\t%s\n", marker, $0, name
            }
        ' | rofi_pick "Input audio"
    )"
    source="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$source" ]] && run_or_notify "Input audio cambiato" pactl set-default-source "$source"
}

audio_menu() {
    while true; do
        local sink_desc source_desc sink_vol source_vol choice
        sink_desc="$(audio_current_sink_desc)"
        source_desc="$(audio_current_source_desc)"
        sink_vol="$(audio_volume_label "@DEFAULT_AUDIO_SINK@")"
        source_vol="$(audio_volume_label "@DEFAULT_AUDIO_SOURCE@")"

        local message_card
        message_card="󰓃 <b><span foreground='${c_accent}'>OUTPUT</span></b>: ${sink_desc}\nVolume: <b><span foreground='${c_yellow}'>${sink_vol}</span></b>\n\n󰍬 <b><span foreground='${c_accent}'>INPUT</span></b>:  ${source_desc}\nVolume: <b><span foreground='${c_yellow}'>${source_vol}</span></b>"

        choice="$(
            {
                printf '󰓃 AUDIO OUTPUT CONTROL\n'
                printf ' ├─ 󰖁  Mute/Unmute Output\n'
                printf ' ├─ 󰕾  Output Volume (%d%%)\n' "$(audio_volume_percent "@DEFAULT_AUDIO_SINK@")"
                printf ' └─ 󰓃  Select Output Device\n'
                
                printf '\n󰍬 AUDIO INPUT CONTROL\n'
                printf ' ├─ 󰍭  Mute/Unmute Microphone\n'
                printf ' ├─ 󰍬  Microphone Volume (%d%%)\n' "$(audio_volume_percent "@DEFAULT_AUDIO_SOURCE@")"
                printf ' └─ 󰍬  Select Input Device\n'
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "Audio" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            *"Mute/Unmute Output"*) wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle ;;
            *"Output Volume"*)
                audio_volume_slider "@DEFAULT_AUDIO_SINK@" "Output Volume" \
                    "Output: $sink_desc" "slider-volume" "output-volume"
                ;;
            *"Mute/Unmute Microphone"*) wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle ;;
            *"Microphone Volume"*)
                audio_volume_slider "@DEFAULT_AUDIO_SOURCE@" "Microphone Volume" \
                    "Input: $source_desc" "slider-mic" "input-volume"
                ;;
            *"Select Output Device"*) audio_choose_sink ;;
            *"Select Input Device"*) audio_choose_source ;;
            *)
                return 0
                ;;
        esac
    done
}

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

battery_menu() {
    local path
    path="$(battery_path)" || {
        printf '%s\n' "$(menu_item "󰌍" "Back")" |
            rofi_pick_msg "$(system_text "Battery")" "$(system_text "No battery detected")" >/dev/null
        return 0
    }

    while true; do
        local choice message title power_saver balanced performance refresh suspend back
        message="$(battery_message "$path")"
        title="$(system_text "Battery")"
        power_saver="$(menu_item "󰐥" "Power Saver")"
        balanced="$(menu_item "󰾅" "Balanced")"
        performance="$(menu_item "󰓅" "Performance")"
        refresh="$(menu_item "󰑓" "Refresh")"
        suspend="$(menu_item "󰤄" "Suspend")"
        back="$(menu_item "󰌍" "Back")"

        choice="$(
            {
                if command -v powerprofilesctl >/dev/null 2>&1; then
                    printf '󰂎 POWER PROFILE\n'
                    printf ' ├─ %s\n' "$power_saver"
                    printf ' ├─ %s\n' "$balanced"
                    printf ' └─ %s\n' "$performance"
                    printf '\n󰐥 POWER ACTIONS\n'
                else
                    printf '󰐥 POWER ACTIONS\n'
                fi
                printf ' ├─ %s\n' "$refresh"
                printf ' └─ %s\n' "$suspend"
                
                printf '\n%s\n' "$back"
            } | rofi_pick_msg "$title" "$message"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "$power_saver") run_or_notify "$(system_text "Power Saver")" powerprofilesctl set power-saver ;;
            "$balanced") run_or_notify "$(system_text "Balanced")" powerprofilesctl set balanced ;;
            "$performance") run_or_notify "$(system_text "Performance")" powerprofilesctl set performance ;;
            "$refresh") continue ;;
            "$suspend") systemctl suspend; return 0 ;;
            "$back")
                back_or_main
                return 0
                ;;
            *) return 0 ;;
        esac
    done
}

xkb_rules_file() {
    if [[ -r /usr/share/X11/xkb/rules/base.lst ]]; then
        printf '/usr/share/X11/xkb/rules/base.lst'
    else
        printf '/usr/share/X11/xkb/rules/evdev.lst'
    fi
}

xkb_layout_codes() {
    local configured
    configured="$(
        awk -F= '
            /^[[:space:]]*kb_layout[[:space:]]*=/ {
                gsub(/[[:space:]]/, "", $2)
                print $2
                exit
            }
        ' "$HOME/.config/hypr/conf/input.conf" 2>/dev/null
    )"

    [[ -n "$configured" ]] || configured="$(localectl status 2>/dev/null | awk -F: '/X11 Layout:/ {gsub(/^[[:space:]]+/, "", $2); print $2; exit}')"
    printf '%s\n' "${configured:-it}" | tr ',' '\n' | awk 'NF && !seen[$0]++ {print}'
}

xkb_raw_description() {
    local code="$1"
    awk -v code="$code" '
        /^! layout/ {in_layout = 1; next}
        /^!/ {in_layout = 0}
        in_layout && $1 == code {
            $1 = ""
            sub(/^[[:space:]]+/, "")
            print
            exit
        }
    ' "$(xkb_rules_file)" 2>/dev/null
}

xkb_description() {
    local code="$1"
    local raw locale_name
    raw="$(xkb_raw_description "$code")"
    raw="${raw:-$code}"
    locale_name="$UI_LOCALE"

    if command -v gettext >/dev/null 2>&1; then
        LC_ALL="$locale_name" gettext -d xkeyboard-config "$raw" 2>/dev/null || printf '%s' "$raw"
    else
        printf '%s' "$raw"
    fi
}

keyboard_active_keymap() {
    if command -v jq >/dev/null 2>&1; then
        hyprctl devices -j 2>/dev/null |
            jq -r '.keyboards[]? | select(.main == true) | .active_keymap // empty' |
            sed -n '1p'
    else
        hyprctl devices 2>/dev/null | awk -F': ' '/active keymap:/ {print $2; exit}'
    fi
}

keyboard_active_code() {
    local active="$1"
    local code raw translated

    while IFS= read -r code; do
        raw="$(xkb_raw_description "$code")"
        translated="$(xkb_description "$code")"
        if [[ "$active" == "$raw" || "$active" == "$translated" ]]; then
            printf '%s' "$code"
            return 0
        fi
    done < <(xkb_layout_codes)

    case "$active" in
        *Italian*) printf 'it' ;;
        *"English (US)"* | *English*) printf 'us' ;;
        *) xkb_layout_codes | sed -n '1p' ;;
    esac
}

keyboard_rows() {
    local active_code="$1"
    local code index marker label desc
    index=0

    while IFS= read -r code; do
        marker=" "
        [[ "$code" == "$active_code" ]] && marker="*"
        label="$(printf '%s' "$code" | tr '[:lower:]' '[:upper:]')"
        desc="$(xkb_description "$code")"
        printf '%s %s  %s\t%s\t%s\t%s\n' "$marker" "$label" "$desc" "$index" "$code" "$desc"
        index=$((index + 1))
    done < <(xkb_layout_codes)
}

keyboard_menu() {
    while true; do
        local active active_code rows labels choice selected index code desc im_status gtk_status
        local title next_layout configure diagnostics back
        active="$(keyboard_active_keymap)"
        active_code="$(keyboard_active_code "$active")"
        rows="$(keyboard_rows "$active_code")"
        labels="$(printf '%s\n' "$rows" | awk -F'\t' 'NF {print $1}')"
        im_status="fcitx5 $(system_text "Inactive")"
        pgrep -x fcitx5 >/dev/null 2>&1 && im_status="fcitx5 $(system_text "Active")"
        gtk_status="GTK_IM_MODULE $(system_text "Unset")"
        [[ -n "${GTK_IM_MODULE:-}" ]] && gtk_status="GTK_IM_MODULE=$GTK_IM_MODULE"
        title="$(system_text "Keyboard")"
        next_layout="$(menu_item "󰒟" "Next")"
        configure="$(menu_item "󰒓" "Settings")"
        diagnostics="$(menu_item "󰋼" "Diagnostics")"
        back="$(menu_item "󰌍" "Back")"

        # Format labels with tree connectors
        local formatted_labels=""
        if [[ -n "$labels" ]]; then
            local count label_lines i
            mapfile -t label_lines <<< "$labels"
            count=${#label_lines[@]}
            for ((i=0; i<count; i++)); do
                if (( i == count - 1 )); then
                    formatted_labels+=$' └─ '"${label_lines[i]}"$'\n'
                else
                    formatted_labels+=$' ├─ '"${label_lines[i]}"$'\n'
                fi
            done
        fi

        choice="$(
            {
                printf ' KEYBOARD LAYOUT\n'
                printf ' └─ %s\n' "$next_layout"
                
                printf '\nAVAILABLE LAYOUTS\n'
                printf '%s' "$formatted_labels"
                
                printf '\n󰒓 TOOLS AND CONFIGURATION\n'
                if command -v fcitx5-configtool >/dev/null 2>&1; then
                    printf ' ├─ %s\n' "$configure"
                    printf ' └─ %s\n' "$diagnostics"
                else
                    printf ' └─ %s\n' "$diagnostics"
                fi
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "$title" "<b>$(system_text "Current")</b>: <b><span foreground='${c_accent}'>$(xkb_description "$active_code")</span></b>\n<b>$(system_text "System")</b>: <span foreground='${c_muted}'>${im_status}</span>\n<span foreground='${c_muted}'>${gtk_status}</span>"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "$next_layout")
                run_or_notify "$(system_text "Keyboard")" hyprctl switchxkblayout all next
                ;;
            "$configure")
                fcitx5-configtool >/dev/null 2>&1 &
                return 0
                ;;
            "$diagnostics")
                rofi_pick_msg "$diagnostics" "$(localectl status 2>/dev/null)\n\nHyprland: ${active:-$(system_text "Unknown")}\n$im_status\n$gtk_status" >/dev/null
                ;;
            "$back")
                back_or_main
                return 0
                ;;
            *)
                selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {print; exit}')"
                [[ -n "$selected" ]] || return 0
                index="$(printf '%s' "$selected" | awk -F'\t' '{print $2}')"
                code="$(printf '%s' "$selected" | awk -F'\t' '{print $3}')"
                desc="$(printf '%s' "$selected" | awk -F'\t' '{print $4}')"
                if hyprctl switchxkblayout all "$index" >/dev/null 2>&1; then
                    notify "$(system_text "Keyboard"): $desc"
                else
                    notify "$(system_text "Keyboard") $(system_text "Failed"): $code"
                fi
                ;;
        esac
    done
}

notifications_menu() {
    "$HOME/.config/anto426/notification_log.sh" >/dev/null 2>&1 &
    mkdir -p "$(dirname "$NOTIFICATIONS_FILE")"
    touch "$NOTIFICATIONS_FILE"

    while true; do
        local count dnd dnd_label history_rows history_labels choice
        count="$(swaync-client -c 2>/dev/null || echo 0)"
        dnd="$(swaync-client -D 2>/dev/null || echo false)"
        dnd_label="off"
        [[ "$dnd" == "true" ]] && dnd_label="active"

        history_rows="$(
            if [[ -s "$NOTIFICATIONS_FILE" ]]; then
                tail -n 8 "$NOTIFICATIONS_FILE" |
                    awk -F'\t' '
                        {
                            app = ($2 == "") ? "App" : $2
                            summary = $3
                            body = $4
                            label = sprintf("󰵙 %s  %s: %s", strftime("%H:%M", $1), app, summary)
                            if (body != "") label = label " - " body
                            if (length(label) > 110) label = substr(label, 1, 107) "..."
                            rows[++n] = label "\t" $1 "\t" app "\t" summary "\t" body
                        }
                        END {
                            for (i = n; i >= 1; i--) print rows[i]
                        }
                    '
            fi
        )"
        history_labels="$(printf '%s\n' "$history_rows" | awk -F'\t' 'NF {print $1}')"
        [[ -z "$history_labels" ]] && history_labels="󰵙 No notifications saved"

        # Format history labels with tree connectors
        local formatted_history=""
        if [[ -n "$history_labels" && "$history_labels" != "󰵙 No notifications saved" ]]; then
            local count hist_lines i
            mapfile -t hist_lines <<< "$history_labels"
            count=${#hist_lines[@]}
            for ((i=0; i<count; i++)); do
                if (( i == count - 1 )); then
                    formatted_history+=$' └─ '"${hist_lines[i]}"$'\n'
                else
                    formatted_history+=$' ├─ '"${hist_lines[i]}"$'\n'
                fi
            done
        else
            formatted_history=" └─ 󰵙 No notifications saved\n"
        fi

        local dnd_color="$c_muted"
        [[ "$dnd" == "true" ]] && dnd_color="$c_red"
        local message_card
        message_card="Active Notifications: <b><span foreground='${c_yellow}'>${count}</span></b>\nDo Not Disturb: <b><span foreground='${dnd_color}'>${dnd_label}</span></b>"

        choice="$(
            {
                printf '󰵙 NOTIFICATIONS MANAGEMENT\n'
                if [[ "$dnd" == "true" ]]; then
                    printf ' ├─ 󰂚  Disable Do Not Disturb\n'
                else
                    printf ' ├─ 󰂛  Enable Do Not Disturb\n'
                fi
                printf ' ├─ 󰵚  Close last notification\n'
                printf ' ├─ 󰆴  Clear all notifications\n'
                printf ' └─ 󰑓  Refresh\n'
                
                printf '\n󰵚 RECENT HISTORY\n'
                printf '%s' "$formatted_history"
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "Notifications" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            *"Disable Do Not Disturb"*) swaync-client -df >/dev/null ;;
            *"Enable Do Not Disturb"*) swaync-client -dn >/dev/null ;;
            *"Close last notification"*)
                swaync-client --close-latest
                [[ -s "$NOTIFICATIONS_FILE" ]] && sed -i '$d' "$NOTIFICATIONS_FILE"
                ;;
            *"Clear all notifications"*)
                swaync-client -C
                : > "$NOTIFICATIONS_FILE"
                ;;
            *"Refresh"*)
                continue
                ;;
            "󰵙 No notifications saved")
                continue
                ;;
            *)
                local selected timestamp app summary body detail detail_choice
                selected="$(printf '%s\n' "$history_rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {print; exit}')"
                [[ -n "$selected" ]] || return 0

                timestamp="$(printf '%s' "$selected" | awk -F'\t' '{print $2}')"
                app="$(printf '%s' "$selected" | awk -F'\t' '{print $3}')"
                summary="$(printf '%s' "$selected" | awk -F'\t' '{print $4}')"
                body="$(printf '%s' "$selected" | awk -F'\t' '{print $5}')"
                detail="App: $app\nTime: $(date -d "@$timestamp" '+%d/%m %H:%M' 2>/dev/null || printf '%s' "$timestamp")\n\n$summary"
                [[ -n "$body" ]] && detail="$detail\n$body"

                while true; do
                    detail_choice="$(
                        {
                            printf '󰵚  NOTIFICATION DETAILS\n'
                            printf ' └─ 󰅍  Copy Text\n'
                            printf '\n󰌍  Back\n'
                        } | rofi_pick_msg "Notification" "$detail"
                    )"

                    [[ -z "$detail_choice" ]] && break
                    if [[ "$detail_choice" == *"Back"* ]]; then
                        break
                    fi

                    if [[ "$detail_choice" != *"├─ "* && "$detail_choice" != *"└─ "* ]]; then
                        continue
                    fi

                    local clean_detail_choice
                    clean_detail_choice="$(printf '%s' "$detail_choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

                    case "$clean_detail_choice" in
                        *"Copy Text"*)
                            printf '%s\n%s\n' "$summary" "$body" | wl-copy 2>/dev/null && notify "Notification text copied"
                            break
                            ;;
                    esac
                done
                ;;
        esac
    done
}

ensure_events_file() {
    mkdir -p "$(dirname "$EVENTS_FILE")"
    if [[ ! -s "$EVENTS_FILE" ]] || ! jq empty "$EVENTS_FILE" >/dev/null 2>&1; then
        printf '[]\n' > "$EVENTS_FILE"
    fi
    if [[ ! -s "$GCAL_EVENTS_FILE" ]] || ! jq empty "$GCAL_EVENTS_FILE" >/dev/null 2>&1; then
        printf '[]\n' > "$GCAL_EVENTS_FILE"
    fi
}

calendar_events_for_date() {
    local date="$1"
    ensure_events_file
    jq -s -r --arg date "$date" '
        def clean: tostring | gsub("[\t\r\n]+"; " ");
        ((.[0] // []) + (.[1] // [])) |
        [.[] | select(.date == $date)] |
        sort_by((if (.all_day // false) then "00:00" else (.start // "00:00") end), (.title // ""))[] |
        [
            ((.start // "") | clean),
            ((.end // "") | clean),
            ((.title // "Evento") | clean),
            ((.description // "") | clean),
            ((.id // "") | clean),
            ((.source // "locale") | clean),
            ((.url // "") | clean),
            ((.all_day // false) | tostring)
        ] | @tsv
    ' "$EVENTS_FILE" "$GCAL_EVENTS_FILE"
}

calendar_events_between() {
    local start_date="$1"
    local end_date="$2"
    ensure_events_file
    jq -s -r --arg start "$start_date" --arg end "$end_date" '
        def clean: tostring | gsub("[\t\r\n]+"; " ");
        ((.[0] // []) + (.[1] // [])) |
        [.[] | select(.date >= $start and .date <= $end)] |
        sort_by(.date, (if (.all_day // false) then "00:00" else (.start // "00:00") end), (.title // ""))[] |
        [
            ((.date // "") | clean),
            ((.start // "") | clean),
            ((.end // "") | clean),
            ((.title // "Evento") | clean),
            ((.description // "") | clean),
            ((.id // "") | clean),
            ((.source // "locale") | clean),
            ((.url // "") | clean),
            ((.all_day // false) | tostring)
        ] | @tsv
    ' "$EVENTS_FILE" "$GCAL_EVENTS_FILE"
}

calendar_rows_for_date() {
    local date="$1"
    calendar_events_for_date "$date" |
        awk -F'\t' -v date="$date" '
            NF {
                source_icon = ($6 == "google") ? "󰊭" : "󰃭"
                when = ($8 == "true" || $1 == "") ? "Tutto il giorno" : $1 (($2 != "") ? "-" $2 : "")
                desc = ($4 == "") ? "" : "  · " $4
                label = sprintf("%s %s  %s%s", source_icon, when, $3, desc)
                if (length(label) > 118) label = substr(label, 1, 115) "..."
                printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", label, $5, $6, date, $1, $2, $3, $4, $7, $8
            }
        '
}

calendar_upcoming_rows() {
    local today end_date
    today="$(date +%F)"
    end_date="$(date -d "$today +30 days" +%F)"
    calendar_events_between "$today" "$end_date" |
        awk -F'\t' '
            NF {
                source_icon = ($7 == "google") ? "󰊭" : "󰃭"
                when = ($9 == "true" || $2 == "") ? "Tutto il giorno" : $2 (($3 != "") ? "-" $3 : "")
                desc = ($5 == "") ? "" : "  · " $5
                label = sprintf("%s %s  %s  %s%s", source_icon, $1, when, $4, desc)
                if (length(label) > 118) label = substr(label, 1, 115) "..."
                printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", label, $6, $7, $1, $2, $3, $4, $5, $8, $9
            }
        '
}

calendar_delete_event_by_id() {
    local id="$1"
    local tmp
    [[ -n "$id" ]] || return 0
    ensure_events_file
    tmp="$(mktemp)"
    jq --arg id "$id" 'map(select(.id != $id))' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"
    notify "Evento locale eliminato"
}

calendar_sync_status() {
    if [[ -s "$GCAL_EVENTS_FILE" ]]; then
        printf 'Google: aggiornato %s' "$(date -r "$GCAL_EVENTS_FILE" '+%d/%m %H:%M' 2>/dev/null)"
    else
        printf 'Google: non sincronizzato'
    fi
}

calendar_sync_google() {
    if [[ ! -x "$REMOTE_SYNC_SCRIPT" ]]; then
        notify "Script sync mancante"
        return 1
    fi

    "$REMOTE_SYNC_SCRIPT" calendar
}

calendar_event_detail_menu() {
    local row="$1"
    local label id source event_date start end title description url all_day source_label when date_label detail choice

    label="$(printf '%s' "$row" | awk -F'\t' '{print $1}')"
    id="$(printf '%s' "$row" | awk -F'\t' '{print $2}')"
    source="$(printf '%s' "$row" | awk -F'\t' '{print $3}')"
    event_date="$(printf '%s' "$row" | awk -F'\t' '{print $4}')"
    start="$(printf '%s' "$row" | awk -F'\t' '{print $5}')"
    end="$(printf '%s' "$row" | awk -F'\t' '{print $6}')"
    title="$(printf '%s' "$row" | awk -F'\t' '{print $7}')"
    description="$(printf '%s' "$row" | awk -F'\t' '{print $8}')"
    url="$(printf '%s' "$row" | awk -F'\t' '{print $9}')"
    all_day="$(printf '%s' "$row" | awk -F'\t' '{print $10}')"

    source_label="Local"
    [[ "$source" == "google" ]] && source_label="Google Calendar"
    date_label="$(date -d "$event_date" '+%a %d/%m/%Y' 2>/dev/null || printf '%s' "$event_date")"
    if [[ "$all_day" == "true" || -z "$start" ]]; then
        when="$date_label, all-day"
    else
        when="$date_label, $start${end:+-$end}"
    fi

    detail="Source: $source_label\nWhen: $when\n\n$title"
    [[ -n "$description" ]] && detail="$detail\n$description"

    choice="$(
        {
            printf '%s\n' "󰅍  Copy Details"
            [[ -n "$url" ]] && printf '%s\n' "󰖟  Open Event"
            [[ "$source" != "google" ]] && printf '%s\n' "󰆴  Delete Local Event"
            printf '%s\n' "󰌍  Back"
        } | rofi_pick_msg "${label%%  *}" "$detail" "$THEME_CALENDAR"
    )"

    case "$choice" in
        "  ──"*) return 0 ;;
        *"Copy Details" | *"Copia dettagli"*)
            printf '%s\n%s\n%s\n' "$title" "$when" "$description" | wl-copy 2>/dev/null || true
            ;;
        *"Open Event" | *"Apri evento"*)
            xdg-open "$url" >/dev/null 2>&1 &
            ;;
        *"Delete Local Event" | *"Elimina evento locale"*)
            calendar_delete_event_by_id "$id"
            ;;
    esac
}

calendar_add_event() {
    local title event_date start_time end_time description normalized id tmp

    title="$(rofi_input "Event Title")"
    [[ -n "$title" ]] || return 0

    event_date="$(rofi_input "Event Date (YYYY-MM-DD)" "$(date +%F)")"
    [[ -n "$event_date" ]] || return 0
    normalized="$(date -d "$event_date" +%F 2>/dev/null)" || {
        notify "Invalid Date"
        return 1
    }

    start_time="$(rofi_input "Start Time (HH:MM)" "$(date +%H:%M)")"
    [[ "$start_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]] || {
        notify "Invalid Start Time"
        return 1
    }

    end_time="$(rofi_input "Optional End Time (HH:MM)")"
    if [[ -n "$end_time" && ! "$end_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]]; then
        notify "Invalid End Time"
        return 1
    fi

    description="$(rofi_input "Optional Description")"
    id="$(date +%s%N)"
    ensure_events_file
    tmp="$(mktemp)"

    jq \
        --arg id "$id" \
        --arg title "$title" \
        --arg date "$normalized" \
        --arg start "$start_time" \
        --arg end "$end_time" \
        --arg description "$description" \
        '. += [{
            id: $id,
            title: $title,
            date: $date,
            start: $start,
            end: $end,
            description: $description
        }]' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"

    notify "Event Added: $title"
}

calendar_show_date() {
    local date="$1"
    local rows labels choice selected

    rows="$(calendar_rows_for_date "$date")"
    if [[ -z "$rows" ]]; then
        printf '%s\n' "󰌍  Back" | rofi_pick_msg "$date" "No events for this date" "$THEME_CALENDAR" >/dev/null
        return 0
    fi

    labels="$(printf '%s\n' "$rows" | awk -F'\t' '{print $1}')"
    choice="$(printf '%s\n' "$labels" | rofi_pick "$date")"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    [[ -n "$selected" ]] && calendar_event_detail_menu "$selected"
}

calendar_show_upcoming() {
    local rows labels choice selected

    rows="$(calendar_upcoming_rows)"
    if [[ -z "$rows" ]]; then
        printf '%s\n' "󰌍  Back" | rofi_pick_msg "Upcoming Events" "No events in the next 30 days" "$THEME_CALENDAR" >/dev/null
        return 0
    fi

    labels="$(printf '%s\n' "$rows" | awk -F'\t' '{print $1}')"
    choice="$(printf '%s\n' "$labels" | rofi_pick "Upcoming 30 days")"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    [[ -n "$selected" ]] && calendar_event_detail_menu "$selected"
}

calendar_delete_event() {
    local item id tmp
    ensure_events_file
    item="$(
        jq -r 'sort_by(.date, .start)[] | "\(.date) \(.start)  \(.title)\t\(.id)"' "$EVENTS_FILE" |
            rofi_pick "Delete Event"
    )"
    id="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$id" ]] || return 0

    tmp="$(mktemp)"
    jq --arg id "$id" 'map(select(.id != $id))' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"
    notify "Local event deleted"
}

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

calendar_month_message() {
    local today month_view events
    today="$(date +%F)"
    
    # Load dynamic theme colors
    local c_accent c_muted c_yellow
    c_accent="$(get_color accent "#8cb8e4")"
    c_muted="$(get_color muted "#b9c4d2")"
    c_yellow="$(get_color yellow "#f9e2af")"

    # Get calendar text
    local day_num
    day_num=$(date +%e | tr -d ' ')
    
    local cal_raw cal_header cal_weekdays cal_body
    cal_raw="$(cal -m)"
    cal_header="$(echo "$cal_raw" | sed -n '1p')"
    cal_weekdays="$(echo "$cal_raw" | sed -n '2p')"
    cal_body="$(echo "$cal_raw" | tail -n +3)"
    
    # Highlight today's date in cal_body, escaping the day boundaries correctly
    cal_body="$(echo "$cal_body" | sed "s/\b${day_num}\b/<b><span foreground='${c_accent}'>${day_num}<\/span><\/b>/")"
    
    # Format headers cleanly
    month_view="<b><span foreground='${c_accent}'>${cal_header}</span></b>\n<span foreground='${c_muted}'>${cal_weekdays}</span>\n${cal_body}"

    # Get events for today
    events="$(
        calendar_events_for_date "$today" |
            awk -F'\t' '
                NF {
                    time_str = ($8 == "true" || $1 == "") ? "All day" : $1
                    print time_str "\t" $3
                }
            ' |
            sed -n '1,5p'
    )"
    
    local events_formatted
    if [[ -z "$events" ]]; then
        events_formatted="<span foreground='${c_muted}'><i>No appointments</i></span>"
    else
        events_formatted=""
        while IFS=$'\t' read -r ev_time ev_title; do
            if [[ -n "$ev_title" ]]; then
                # Clean event rendering: "12:00  Meeting Name"
                if [[ "$ev_time" == "All day" ]]; then
                    events_formatted="${events_formatted}<span foreground='${c_muted}'>all-day</span>  ${ev_title}\n"
                else
                    events_formatted="${events_formatted}<span foreground='${c_yellow}'>${ev_time}</span>  ${ev_title}\n"
                fi
            fi
        done <<< "$events"
    fi
    
    # Reassemble a beautifully clean, minimalist card
    printf '%b\n\n<b><span foreground="%s">TODAY'\''S APPOINTMENTS</span></b>\n%b' \
        "$month_view" \
        "$c_accent" \
        "$events_formatted"
}

calendar_menu() {
    while true; do
        local today month choice picked_date message
        today="$(date +%F)"
        month="$(date '+%B %Y')"
        message="$(calendar_month_message)"

        choice="$(
            {
                printf '󰃭 EVENTS NAVIGATION\n'
                printf ' ├─ 󰃭 Today'\''s events\n'
                printf ' ├─ 󰔚 Next 30 days\n'
                printf ' └─ 󰥔 Choose date\n'
                
                printf '\n CALENDAR MANAGEMENT\n'
                printf ' ├─  Add local event\n'
                printf ' ├─ 󰧭 Delete local event\n'
                printf ' └─ 󰑓 Sync Google Calendar\n'
                
                printf '\n󰒓 TOOLS AND WEB\n'
                printf ' ├─ 󰖟  Open Google Calendar\n'
                printf ' └─ 󰒓  Config sync\n'
                
                printf '\n󰌍  Back\n'
            } | rofi_pick_msg "$month" "$message" "$THEME_CALENDAR"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "󰑓 Sync Google Calendar") calendar_sync_google ;;
            " Add local event") calendar_add_event ;;
            "󰃭 Today's events") calendar_show_date "$today" ;;
            "󰔚 Next 30 days") calendar_show_upcoming ;;
            "󰥔 Choose date")
                picked_date="$(rofi_input "Date (YYYY-MM-DD)" "$today")"
                [[ -n "$picked_date" ]] && calendar_show_date "$(date -d "$picked_date" +%F 2>/dev/null || printf '%s' "$today")"
                ;;
            "󰧭 Delete local event") calendar_delete_event ;;
            "󰖟 Open Google Calendar") xdg-open "https://calendar.google.com/calendar/u/0/r" >/dev/null 2>&1 & ;;
            "󰒓 Config sync") "$REMOTE_SYNC_SCRIPT" config ;;
            *)
                return 0
                ;;
        esac
    done
}

power_confirm() {
    local prompt="$1"
    local message="$2"
    local choice
    local confirm cancel
    confirm="$(menu_item "󰄬" "Confirm")"
    cancel="$(menu_item "󰜺" "Cancel")"

    choice="$(
        printf '%s\n' \
            "$confirm" \
            "$cancel" |
            rofi_pick_msg "$prompt" "$message"
    )"

    [[ "$choice" == "$confirm" ]]
}

power_menu() {
    while true; do
        local choice
        local title lock logout suspend reboot poweroff back

        title="$(system_text "Power Off")"
        lock="$(menu_item "󰌾" "Lock")"
        logout="$(menu_item "󰍃" "Log Out")"
        suspend="$(menu_item "󰤄" "Suspend")"
        reboot="$(menu_item "󰜉" "Restart")"
        poweroff="$(menu_item "󰐥" "Power Off")"
        back="$(menu_item "󰌍" "Back")"
        local message_card
        message_card="<b><span foreground='${c_accent}'>Hyprland Session Control</span></b>\n<span foreground='${c_muted}'>$(system_text "Select an action")</span>"

        choice="$(
            {
                printf '󰌾 SESSION CONTROL\n'
                printf ' ├─ %s\n' "$lock"
                printf ' ├─ %s\n' "$logout"
                printf ' └─ %s\n' "$suspend"
                
                printf '\n󰐥 SYSTEM CONTROL\n'
                printf ' ├─ %s\n' "$reboot"
                printf ' └─ %s\n' "$poweroff"
                
                printf '\n%s\n' "$back"
            } | rofi_pick_msg "$title" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" == *"$back"* || "$choice" == *"Back"* ]]; then
            back_or_main
            return 0
        fi
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            "$lock")
                hyprlock
                return 0
                ;;
            "$logout")
                power_confirm "$logout" "$(system_text "Log out?")" &&
                    hyprctl dispatch exit 0
                return 0
                ;;
            "$suspend")
                systemctl suspend
                return 0
                ;;
            "$reboot")
                power_confirm "$reboot" "$(system_text "Restart?")" &&
                    systemctl reboot
                return 0
                ;;
            "$poweroff")
                power_confirm "$poweroff" "$(system_text "Power off?")" &&
                    systemctl poweroff
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}

main_menu() {
    while true; do
        local choice

        choice="$(
            {
                printf '󱊖 QUICK CONTROLS\n'
                printf ' ├─ 󰤨  Wi-Fi Connection\n'
                printf ' ├─ 󰂯  Bluetooth Devices\n'
                printf ' ├─ 󰓃  Audio Adjustments (Volume/Input)\n'
                printf ' ├─ 󰃠  Screen Brightness\n'
                printf ' └─ 󰂎  Battery Information\n'
                
                printf '\n󰒔 SYSTEM CONFIGURATION\n'
                printf ' ├─   Keyboard Layout\n'
                printf ' ├─ 󰵙  Notification History\n'
                printf ' ├─ 󰃭  Calendar and Events\n'
                printf ' ├─ 󰍹  Project Screen (Display)\n'
                printf ' └─ 󱂬  Floating Window Manager\n'
                
                printf '\n󰏘 PERSONALIZATION\n'
                printf ' ├─ 󰸉  Select Wallpaper\n'
                printf ' └─ 󱓞  Desktop Widgets Management\n'
                
                printf '\n󰐥 SYSTEM SHUTDOWN\n'
                printf ' └─ 󰤆  Power Menu\n'
            } | rofi_pick "$(system_text "Settings")"
        )"

        [[ -z "$choice" ]] && return 0
        if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
            continue
        fi
        local clean_choice
        clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

        case "$clean_choice" in
            *"Bluetooth"*) MENU_STATE="bluetooth"; return 0 ;;
            *"Wi-Fi"* | *"Wi-Fi Connection"*) MENU_STATE="wifi"; return 0 ;;
            *"Audio"* | *"Audio Adjustments"*) MENU_STATE="audio"; return 0 ;;
            *"Luminosità"* | *"Brightness"*) ANTO426_MENU_PARENT=control "$HOME/.config/anto426/brightness_menu.sh"; return 0 ;;
            *"Batteria"* | *"Battery"*) MENU_STATE="battery"; return 0 ;;
            *"Tastiera"* | *"Keyboard"*) MENU_STATE="keyboard"; return 0 ;;
            *"Notifiche"* | *"Notification"*) MENU_STATE="notifications"; return 0 ;;
            *"Calendario"* | *"Calendar"*) MENU_STATE="calendar"; return 0 ;;
            *"Sfondo"* | *"Wallpaper"* | *"Select Wallpaper"*) ANTO426_MENU_PARENT=control "$HOME/.config/anto426/wallpaper_select.sh"; return 0 ;;
            *"Proietta Schermo"* | *"Project"* | *"Display"*) ANTO426_MENU_PARENT=control "$HOME/.config/anto426/projection_menu.sh"; return 0 ;;
            *"Gestione Widget"* | *"Widgets"*) "$HOME/.config/anto426/widgets.sh" arrange; return 0 ;;
            *"Floating Manager"* | *"Floating"*) ANTO426_MENU_PARENT=control "$HOME/.config/anto426/floating_manager.sh" menu; return 0 ;;
            *"Menu Spegnimento"* | *"Power"* | *"Power Menu"*) MENU_STATE="power"; return 0 ;;
            *) return 0 ;;
        esac
    done
}

# FSM state engine runner loop
while [[ -n "$MENU_STATE" ]]; do
    case "$MENU_STATE" in
        bluetooth) MENU_STATE=""; bluetooth_menu ;;
        wifi) MENU_STATE=""; wifi_menu ;;
        audio) MENU_STATE=""; audio_menu ;;
        battery) MENU_STATE=""; battery_menu ;;
        keyboard) MENU_STATE=""; keyboard_menu ;;
        notifications) MENU_STATE=""; notifications_menu ;;
        calendar) MENU_STATE=""; calendar_menu ;;
        power) MENU_STATE=""; power_menu ;;
        main | control) MENU_STATE=""; main_menu ;;
        *) MENU_STATE="" ;;
    esac
done
