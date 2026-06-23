#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

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
            security = ($3 == "" || $3 == "--") ? "open" : $3
            secure_label = (security == "open") ? "" : " (Protetta)"
            active_str = ($1 == "yes") ? " (Connesso)" : ""
            label = sprintf("%s%s%s  (%d%%)", $2, secure_label, active_str, sig)

            icon = "network-wireless-signal-excellent"
            if (security != "open") {
                if (sig >= 80) icon = "network-wireless-signal-excellent-secure"
                else if (sig >= 60) icon = "network-wireless-signal-good-secure"
                else if (sig >= 40) icon = "network-wireless-signal-ok-secure"
                else icon = "network-wireless-signal-weak-secure"
            } else {
                if (sig >= 80) icon = "network-wireless-signal-excellent"
                else if (sig >= 60) icon = "network-wireless-signal-good"
                else if (sig >= 40) icon = "network-wireless-signal-ok"
                else icon = "network-wireless-signal-weak"
            }
            if ($1 == "yes") {
                icon = "network-wireless-connected"
            }

            labels[ssid] = label
            securities[ssid] = security
            actives[ssid] = $1
            icons[ssid] = icon
            if ($1 == "yes") active_seen[ssid] = 1
        }
        END {
            for (i = 1; i <= n; i++) {
                ssid = order[i]
                printf "%s\t%s\t%s\t%s\t%s\n", labels[ssid], ssid, securities[ssid], actives[ssid], icons[ssid]
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
        local conn_label
        if [[ "$active" == "yes" ]]; then
            conn_label="$(system_text "Disconnect")"
        else
            conn_label="$(system_text "Connect")"
        fi

        choice="$(
            {
                printf '%s\0icon\x1fnetwork-wireless\n' "$conn_label"
                if [[ "$has_profile" == "true" ]]; then
                    printf '%s\0icon\x1fuser-trash\n' "$(system_text "Forget network")"
                fi
                printf '%s\0icon\x1fdialog-information\n' "$(system_text "Info")"
                printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
            } | rofi_pick "$ssid"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Connect")")
                if [[ "$has_profile" == "true" ]]; then
                    wifi_connect_saved "$ssid" "$security" "$profile_name"
                else
                    wifi_connect_new "$ssid" "$security"
                fi
                return 0
                ;;
            "$(system_text "Disconnect")")
                notify "Disconnecting from $ssid..."
                run_or_notify "Disconnection from $ssid" nmcli connection down id "${profile_name:-$ssid}"
                wifi_cache_update >/dev/null 2>&1 || true
                return 0
                ;;
            "$(system_text "Forget network")")
                run_or_notify "Network forgotten" nmcli connection delete id "${profile_name:-$ssid}"
                wifi_cache_update >/dev/null 2>&1 || true
                return 0
                ;;
            "$(system_text "Info")")
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
                                printf 'SSID: %s\0icon\x1fnetwork-wireless\n' "${ssid}"
                                printf 'UUID: %s\0icon\x1fnetwork-wireless-encrypted\n' "${uuid:---}"
                                printf 'Device: %s\0icon\x1fnetwork-wired\n' "${device:---}"
                                printf 'State: %s\0icon\x1finfo\n' "${state}"
                                
                                if [[ -n "$ip4" && "$ip4" != "--" ]]; then
                                    printf 'IPv4: %s\0icon\x1fnetwork-wired\n' "${ip4}"
                                    printf '%s\0icon\x1fedit-copy\n' "$(system_text "Copy IPv4")"
                                    [[ -n "$gw4" && "$gw4" != "--" ]] && printf 'IPv4 Gateway: %s\0icon\x1fnetwork-wired\n' "${gw4}"
                                    [[ -n "$dns4" && "$dns4" != "--" ]] && printf 'IPv4 DNS: %s\0icon\x1fnetwork-wired\n' "${dns4}"
                                fi
                                
                                if [[ -n "$ip6" && "$ip6" != "--" ]]; then
                                    printf 'IPv6: %s\0icon\x1fnetwork-wired\n' "${ip6}"
                                    [[ -n "$gw6" && "$gw6" != "--" ]] && printf 'IPv6 Gateway: %s\0icon\x1fnetwork-wired\n' "${gw6}"
                                    [[ -n "$dns6" && "$dns6" != "--" ]] && printf 'IPv6 DNS: %s\0icon\x1fnetwork-wired\n' "${dns6}"
                                fi
                                printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
                            } | rofi -dmenu -i -show-icons -p "Network Info" -theme "$THEME_MENU"
                        )"
                        
                        [[ -z "$info_choice" ]] && break
                        if [[ "$info_choice" == "$(system_text "Back")" ]]; then
                            break
                        fi
                        if [[ "$info_choice" == "$(system_text "Copy IPv4")" ]]; then
                            if command -v wl-copy >/dev/null 2>&1; then
                                printf '%s' "$ip4" | wl-copy && notify "IPv4 copiato"
                            else
                                notify "wl-copy non disponibile"
                            fi
                        fi
                    done
                else
                    while true; do
                        local info_choice
                        info_choice="$(
                            {
                                printf 'SSID: %s\0icon\x1fnetwork-wireless\n' "${ssid}"
                                printf 'Status: Unsaved / Available\0icon\x1finfo\n'
                                printf 'Security: %s\0icon\x1fnetwork-wireless-encrypted\n' "${security:---}"
                                printf '%s\0icon\x1fedit-copy\n' "$(system_text "Copy SSID")"
                                printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
                            } | rofi -dmenu -i -show-icons -p "Network Info" -theme "$THEME_MENU"
                        )"
                        [[ -z "$info_choice" ]] && break
                        if [[ "$info_choice" == "$(system_text "Back")" ]]; then
                            break
                        fi
                        if [[ "$info_choice" == "$(system_text "Copy SSID")" ]]; then
                            if command -v wl-copy >/dev/null 2>&1; then
                                printf '%s' "$ssid" | wl-copy && notify "SSID copiato"
                            else
                                notify "wl-copy non disponibile"
                            fi
                        fi
                    done
                fi
                ;;
            *)
                if [[ "$choice" == "$(system_text "Back")" ]]; then
                    return 0
                fi
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

        local state_color="$c_red"
        [[ "$state" == "on" ]] && state_color="$c_green"
        local conn_color="$c_muted"
        [[ "$connected" != "no network" ]] && conn_color="$c_cyan"
        local message_card
        message_card="Status: <b><span foreground='${state_color}'>Wi-Fi ${state}</span></b>\nConnected: <b><span foreground='${conn_color}'>${connected}</span></b>\n<span foreground='${c_muted}'>${cache_status}</span>"

        local toggle_label
        if [[ "$state" == "on" ]]; then
            toggle_label="$(system_text "Disable Wi-Fi")"
        else
            toggle_label="$(system_text "Enable Wi-Fi")"
        fi

        choice="$(
            {
                printf '%s\0icon\x1fnetwork-wireless\n' "$toggle_label"
                printf '%s\0icon\x1fsystem-search\n' "$(system_text "Scan networks")"
                if command -v nm-connection-editor >/dev/null 2>&1; then
                    printf '%s\0icon\x1fpreferences-system-network\n' "$(system_text "Network settings")"
                fi
                
                if [[ -n "$network_rows" ]]; then
                    while IFS=$'\t' read -r label ssid security active icon; do
                        [[ -n "$ssid" ]] || continue
                        printf '%s\0icon\x1f%s\n' "$label" "${icon:-network-wireless}"
                    done <<< "$network_rows"
                fi
                
                printf '%s\0icon\x1fgo-previous\n' "$(system_text "Back")"
            } | rofi_pick_msg "Wi-Fi" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0
        local clean_choice="$choice"

        case "$clean_choice" in
            "$(system_text "Enable Wi-Fi")" | "$(system_text "Disable Wi-Fi")")
                wifi_toggle
                wifi_cache_refresh_background true
                ;;
            "$(system_text "Scan networks")")
                wifi_rescan
                ;;
            "$(system_text "Network settings")")
                open_or_notify "Network settings" nm-connection-editor
                return 0
                ;;
            *)
                if [[ "$clean_choice" == "$(system_text "Back")" ]]; then
                    back_or_main
                    return 0
                fi
                local dev_item
                dev_item="$(printf '%s\n' "$network_rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {printf "%s\t%s\t%s\t%s", $1, $2, $3, $4; exit}')"
                [[ -n "$dev_item" ]] && wifi_device_menu "$dev_item"
                ;;
        esac
    done
}

wifi_menu
