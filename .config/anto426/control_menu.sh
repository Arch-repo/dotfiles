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
    printf '%s %s' "$icon" "$(system_text "$msgid")"
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
    rofi -dmenu -password -p "$prompt" -theme "$THEME_MENU"
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
                "Stato: $status" \
                "󰌷 Connetti" \
                "󰌸 Disconnetti" \
                "󰖔 Associa" \
                "󰗠 Autorizza" \
                "󰆴 Rimuovi" \
                "󰋼 Informazioni" \
                "󰌍 Indietro" |
                rofi_pick "$label"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰌷 Connetti")
                notify "Connessione in corso a $label..."
                run_or_notify "Connessione Bluetooth" bluetoothctl connect "$mac"
                return 0
                ;;
            "󰌸 Disconnetti")
                notify "Disconnessione in corso da $label..."
                run_or_notify "Disconnessione Bluetooth" bluetoothctl disconnect "$mac"
                return 0
                ;;
            "󰖔 Associa")
                run_or_notify "Associazione Bluetooth" bluetoothctl pair "$mac"
                ;;
            "󰗠 Autorizza")
                run_or_notify "Autorizzazione Bluetooth" bluetoothctl trust "$mac"
                ;;
            "󰆴 Rimuovi")
                run_or_notify "Dispositivo rimosso" bluetoothctl remove "$mac"
                return 0
                ;;
            "󰋼 Informazioni")
                bluetoothctl info "$mac" 2>&1 | rofi_pick "Info Bluetooth" >/dev/null
                ;;
            "󰌍 Indietro")
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
        state="spento"
        [[ "$powered" == "yes" ]] && state="acceso"
        [[ -z "$powered" ]] && state="non disponibile"

        device_rows="$(bluetooth_device_rows)"
        connected_bt="$(printf '%s\n' "$device_rows" | awk -F'\t' '$4 == "yes" {print $5; exit}')"
        [[ -z "$connected_bt" ]] && connected_bt="nessuno"

        devices="$(printf '%s\n' "$device_rows" | awk -F'\t' '{print $1}')"
        [[ -z "$devices" ]] && devices="󰂲 Nessun dispositivo trovato"

        choice="$(
            {
                printf '%s\n' "󰐕 Attiva/Disattiva"
                printf '%s\n' "󰑓 Scansiona dispositivi"
                printf '%s\n' "$devices"
                printf '%s\n' "󰌍 Indietro"
            } | rofi_pick_msg "Bluetooth" "Stato: Bluetooth $state\nConnesso: $connected_bt"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰐕 Attiva/Disattiva")
                bluetooth_toggle
                ;;
            "󰑓 Scansiona dispositivi")
                bluetooth_scan
                ;;
            "󰌍 Indietro")
                back_or_main
                return 0
                ;;
            "󰂲 Nessun dispositivo trovato")
                continue
                ;;
            *)
                local dev_item
                dev_item="$(printf '%s\n' "$device_rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {printf "%s\t%s\t%s", $1, $2, $3; exit}')"
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
            label = sprintf("%s  %s%s%s", marker, $2, lock, active_str)

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

    if raw_list="$(timeout 4s nmcli -t -f ACTIVE,SSID,SECURITY,SIGNAL dev wifi list --rescan no 2>/dev/null)"; then
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
    raw_list="$(timeout 1s nmcli -t -f ACTIVE,SSID,SECURITY,SIGNAL dev wifi list --rescan no 2>/dev/null || true)"
    printf '%s\n' "$raw_list" | wifi_network_rows
}

wifi_cache_status() {
    local age
    age="$(wifi_cache_age 2>/dev/null)" || {
        printf 'Lista: aggiornamento in corso'
        return 0
    }

    if (( age < 60 )); then
        printf 'Lista: aggiornata %ss fa' "$age"
    else
        printf 'Lista: aggiornata %sm fa' "$((age / 60))"
    fi
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
    local password output

    if [[ "$security" == *"802.1X"* ]]; then
        notify "Rete 802.1X: serve un profilo già configurato"
        return 1
    fi

    if [[ "$security" == "open" ]]; then
        notify "Connessione in corso a $ssid..."
        if output="$(nmcli -w 20 dev wifi connect "$ssid" 2>&1)"; then
            notify "Connesso a $ssid"
        else
            notify "Connessione a $ssid fallita: $output"
        fi
        return
    fi

    password="$(rofi_password "Password Wi-Fi ($ssid)")"
    [[ -n "$password" ]] || return 0

    notify "Connessione in corso a $ssid..."
    run_or_notify "Connessione a $ssid" nmcli -w 30 dev wifi connect "$ssid" password "$password"
}

wifi_device_menu() {
    local item="$1"
    local label ssid security choice active has_profile

    label="$(printf '%s' "$item" | awk -F'\t' '{print $1}')"
    ssid="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    security="$(printf '%s' "$item" | awk -F'\t' '{print $3}')"
    active="$(printf '%s' "$item" | awk -F'\t' '{print $4}')"
    [[ -n "$ssid" ]] || return 0

    if nmcli -g NAME connection show 2>/dev/null | grep -Fxq "$ssid"; then
        has_profile=true
    else
        has_profile=false
    fi

    while true; do
        local options=()
        if [[ "$active" == "yes" ]]; then
            options+=("󰌸 Disconnetti")
        else
            options+=("󰌷 Connetti")
        fi

        if [[ "$has_profile" == "true" ]]; then
            options+=("󰆴 Dimentica rete")
        fi

        options+=("󰋼 Informazioni")
        options+=("󰌍 Indietro")

        local options_str
        options_str="$(printf '%s\n' "${options[@]}")"

        choice="$(printf '%s\n' "$options_str" | rofi_pick "$ssid")"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰌷 Connetti")
                if [[ "$has_profile" == "true" ]]; then
                    notify "Connessione in corso a $ssid..."
                    run_or_notify "Connessione a $ssid" nmcli connection up id "$ssid"
                else
                    wifi_connect_new "$ssid" "$security"
                fi
                return 0
                ;;
            "󰌸 Disconnetti")
                notify "Disconnessione in corso da $ssid..."
                run_or_notify "Disconnessione da $ssid" nmcli connection down id "$ssid"
                return 0
                ;;
            "󰆴 Dimentica rete")
                run_or_notify "Rete dimenticata" nmcli connection delete id "$ssid"
                return 0
                ;;
            "󰋼 Informazioni")
                if [[ "$has_profile" == "true" ]]; then
                    nmcli connection show id "$ssid" 2>&1 | rofi_pick "Info Rete: $ssid" >/dev/null
                else
                    printf 'Rete non salvata\nSSID: %s\nSicurezza: %s\n' "$ssid" "$security" | rofi_pick "Info Rete: $ssid" >/dev/null
                fi
                ;;
            "󰌍 Indietro")
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
        state="spenta"
        [[ "$enabled" == "enabled" ]] && state="accesa"
        [[ -z "$enabled" ]] && state="non disponibile"

        connected="$(wifi_connected_ssid)"
        [[ -z "$connected" ]] && connected="nessuna rete"

        network_rows="$(wifi_cached_rows)"
        networks="$(printf '%s\n' "$network_rows" | awk -F'\t' '{print $1}')"
        [[ -z "$networks" ]] && networks="󰤭 Reti in caricamento"
        cache_status="$(wifi_cache_status)"

        choice="$(
            {
                printf '%s\n' "󰐕 Attiva/Disattiva"
                printf '%s\n' "󰑓 Scansiona reti"
                printf '%s\n' "$networks"
                printf '%s\n' "󰌍 Indietro"
            } | rofi_pick_msg "Wi-Fi" "Stato: Wi-Fi $state\nConnesso: $connected\n$cache_status"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰐕 Attiva/Disattiva")
                wifi_toggle
                wifi_cache_refresh_background true
                ;;
            "󰑓 Scansiona reti")
                wifi_rescan
                ;;
            "󰌍 Indietro")
                back_or_main
                return 0
                ;;
            "󰤭 Reti in caricamento")
                wifi_cache_refresh_background true
                continue
                ;;
            *)
                local dev_item
                dev_item="$(printf '%s\n' "$network_rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {printf "%s\t%s\t%s\t%s", $1, $2, $3, $4; exit}')"
                [[ -n "$dev_item" ]] && wifi_device_menu "$dev_item"
                ;;
        esac
    done
}

audio_volume() {
    wpctl get-volume "$1" 2>/dev/null | sed 's/^Volume: //'
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

audio_slider_bar() {
    local value="$1"
    local filled=$((value / 10))
    local bar=""
    local i

    for ((i = 0; i < 10; i++)); do
        if ((i < filled)); then
            bar+="━"
        else
            bar+="─"
        fi
    done

    printf '%s' "$bar"
}

audio_volume_slider() {
    local target="$1"
    local prompt="$2"
    local message="$3"
    local current selected choice value pct marker

    current="$(audio_volume_percent "$target")"
    [[ -n "$current" ]] || current=0
    selected=$(((current + 2) / 5))
    message="$(printf '%b' "$message")"

    choice="$(
        for pct in $(seq 0 5 100); do
            marker=" "
            ((pct == selected * 5)) && marker="*"
            printf '%s %3d%%  %s\n' "$marker" "$pct" "$(audio_slider_bar "$pct")"
        done |
            rofi -dmenu -i -matching fuzzy \
                -p "$prompt" \
                -mesg "$message" \
                -selected-row "$selected" \
                -theme "$THEME_MENU"
    )"

    [[ -z "$choice" ]] && return 0

    if [[ "$choice" =~ ([0-9]{1,3}) ]]; then
        value="${BASH_REMATCH[1]}"
        value=$((10#$value))
        ((value < 0)) && value=0
        ((value > 100)) && value=100
        run_or_notify "$prompt impostato a $value%" wpctl set-volume -l 1 "$target" "$value%"
    fi
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
        sink_vol="$(audio_volume "@DEFAULT_AUDIO_SINK@")"
        source_vol="$(audio_volume "@DEFAULT_AUDIO_SOURCE@")"

        choice="$(
            printf '%s\n' \
                "󰖁 Silenzia/Attiva Output" \
                "󰕾 Regola volume Output" \
                "󰕾 Output +5%" \
                "󰕿 Output -5%" \
                "󰍭 Silenzia/Attiva Microfono" \
                "󰍬 Regola volume Microfono" \
                "󰍬 Microfono +5%" \
                "󰍬 Microfono -5%" \
                "󰓃 Seleziona dispositivo Output" \
                "󰍬 Seleziona dispositivo Input" \
                "󰌍 Indietro" |
                rofi_pick_msg "Audio" "Output: $sink_desc\nVolume: $sink_vol\n\nInput:  $source_desc\nVolume: $source_vol"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            *"Silenzia/Attiva Output") wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle ;;
            *"Regola volume Output") audio_volume_slider "@DEFAULT_AUDIO_SINK@" "Volume Output" "Output: $sink_desc\nVolume: $sink_vol" ;;
            *"Output +5%") wpctl set-volume -l 1 @DEFAULT_AUDIO_SINK@ 5%+ ;;
            *"Output -5%") wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%- ;;
            *"Silenzia/Attiva Microfono") wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle ;;
            *"Regola volume Microfono") audio_volume_slider "@DEFAULT_AUDIO_SOURCE@" "Volume Microfono" "Input: $source_desc\nVolume: $source_vol" ;;
            *"Microfono +5%") wpctl set-volume -l 1 @DEFAULT_AUDIO_SOURCE@ 5%+ ;;
            *"Microfono -5%") wpctl set-volume @DEFAULT_AUDIO_SOURCE@ 5%- ;;
            *"Seleziona dispositivo Output") audio_choose_sink ;;
            *"Seleziona dispositivo Input") audio_choose_source ;;
            "󰌍 Indietro")
                back_or_main
                return 0
                ;;
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

    printf '%s: %s%%\n%s: %s\n%s: %s\n' \
        "$(system_text "Battery")" "${capacity:-?}" \
        "$(system_text "Status")" "$status_label" \
        "$(system_text "Power")" "$power_state"
    [[ -n "$estimate" ]] && printf '%s: %s\n' "$(system_text "Time")" "$estimate"
    printf '%s: %s\n%s: %s' \
        "$(system_text "Profile")" "$profile" \
        "$(system_text "Health")" "$health"
    [[ -n "$manufacturer$model" ]] && printf '\n%s: %s %s' "$(system_text "Battery")" "$manufacturer" "$model"
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
                    printf '%s\n' "$power_saver"
                    printf '%s\n' "$balanced"
                    printf '%s\n' "$performance"
                fi
                printf '%s\n' "$refresh"
                printf '%s\n' "$suspend"
                printf '%s\n' "$back"
            } | rofi_pick_msg "$title" "$message"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
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

        choice="$(
            {
                printf '%s\n' "$next_layout"
                printf '%s\n' "$labels"
                command -v fcitx5-configtool >/dev/null 2>&1 && printf '%s\n' "$configure"
                printf '%s\n' "$diagnostics"
                printf '%s\n' "$back"
            } | rofi_pick_msg "$title" "$(system_text "Current"): $(xkb_description "$active_code")\n$(system_text "System"): $im_status\n$gtk_status"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
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
                selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
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
        dnd_label="spento"
        [[ "$dnd" == "true" ]] && dnd_label="attivo"

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
        [[ -z "$history_labels" ]] && history_labels="󰵙 Nessuna notifica salvata"

        choice="$(
            {
                if [[ "$dnd" == "true" ]]; then
                    printf '%s\n' "󰂚 Disattiva Non Disturbare"
                else
                    printf '%s\n' "󰂛 Attiva Non Disturbare"
                fi
                printf '%s\n' "󰵚 Chiudi ultima notifica"
                printf '%s\n' "󰆴 Cancella tutte le notifiche"
                printf '%s\n' "󰑓 Aggiorna"
                printf '%s\n' "$history_labels"
                printf '%s\n' "󰌍 Indietro"
            } | rofi_pick_msg "Notifiche" "Nel menu: $count\nNon disturbare: $dnd_label"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            *"Attiva Non Disturbare") swaync-client -dn >/dev/null ;;
            *"Disattiva Non Disturbare") swaync-client -df >/dev/null ;;
            *"Chiudi ultima notifica")
                swaync-client --close-latest
                [[ -s "$NOTIFICATIONS_FILE" ]] && sed -i '$d' "$NOTIFICATIONS_FILE"
                ;;
            *"Cancella tutte le notifiche")
                swaync-client -C
                : > "$NOTIFICATIONS_FILE"
                ;;
            *"Aggiorna")
                continue
                ;;
            "󰵙 Nessuna notifica salvata")
                continue
                ;;
            "󰌍 Indietro")
                back_or_main
                return 0
                ;;
            *)
                local selected timestamp app summary body detail detail_choice
                selected="$(printf '%s\n' "$history_rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
                [[ -n "$selected" ]] || return 0

                timestamp="$(printf '%s' "$selected" | awk -F'\t' '{print $2}')"
                app="$(printf '%s' "$selected" | awk -F'\t' '{print $3}')"
                summary="$(printf '%s' "$selected" | awk -F'\t' '{print $4}')"
                body="$(printf '%s' "$selected" | awk -F'\t' '{print $5}')"
                detail="App: $app\nOra: $(date -d "@$timestamp" '+%d/%m %H:%M' 2>/dev/null || printf '%s' "$timestamp")\n\n$summary"
                [[ -n "$body" ]] && detail="$detail\n$body"

                detail_choice="$(
                    printf '%s\n' \
                        "󰅍 Copia testo" \
                        "󰌍 Indietro" |
                        rofi_pick_msg "Notifica" "$detail"
                )"

                case "$detail_choice" in
                    *"Copia testo") printf '%s\n%s\n' "$summary" "$body" | wl-copy 2>/dev/null || true ;;
                esac
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

    source_label="Locale"
    [[ "$source" == "google" ]] && source_label="Google Calendar"
    date_label="$(date -d "$event_date" '+%a %d/%m/%Y' 2>/dev/null || printf '%s' "$event_date")"
    if [[ "$all_day" == "true" || -z "$start" ]]; then
        when="$date_label, tutto il giorno"
    else
        when="$date_label, $start${end:+-$end}"
    fi

    detail="Fonte: $source_label\nQuando: $when\n\n$title"
    [[ -n "$description" ]] && detail="$detail\n$description"

    choice="$(
        {
            printf '%s\n' "󰅍 Copia dettagli"
            [[ -n "$url" ]] && printf '%s\n' "󰖟 Apri evento"
            [[ "$source" != "google" ]] && printf '%s\n' "󰆴 Elimina evento locale"
            printf '%s\n' "󰌍 Indietro"
        } | rofi_pick_msg "${label%%  *}" "$detail" "$THEME_CALENDAR"
    )"

    case "$choice" in
        *"Copia dettagli")
            printf '%s\n%s\n%s\n' "$title" "$when" "$description" | wl-copy 2>/dev/null || true
            ;;
        *"Apri evento")
            xdg-open "$url" >/dev/null 2>&1 &
            ;;
        *"Elimina evento locale")
            calendar_delete_event_by_id "$id"
            ;;
    esac
}

calendar_add_event() {
    local title event_date start_time end_time description normalized id tmp

    title="$(rofi_input "Titolo evento")"
    [[ -n "$title" ]] || return 0

    event_date="$(rofi_input "Data evento (YYYY-MM-DD)" "$(date +%F)")"
    [[ -n "$event_date" ]] || return 0
    normalized="$(date -d "$event_date" +%F 2>/dev/null)" || {
        notify "Data non valida"
        return 1
    }

    start_time="$(rofi_input "Ora inizio (HH:MM)" "$(date +%H:%M)")"
    [[ "$start_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]] || {
        notify "Ora inizio non valida"
        return 1
    }

    end_time="$(rofi_input "Ora fine opzionale (HH:MM)")"
    if [[ -n "$end_time" && ! "$end_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]]; then
        notify "Ora fine non valida"
        return 1
    fi

    description="$(rofi_input "Descrizione opzionale")"
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

    notify "Evento aggiunto: $title"
}

calendar_show_date() {
    local date="$1"
    local rows labels choice selected

    rows="$(calendar_rows_for_date "$date")"
    if [[ -z "$rows" ]]; then
        printf '%s\n' "󰌍 Indietro" | rofi_pick_msg "$date" "Nessun evento per questa data" "$THEME_CALENDAR" >/dev/null
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
        printf '%s\n' "󰌍 Indietro" | rofi_pick_msg "Prossimi eventi" "Nessun evento nei prossimi 30 giorni" "$THEME_CALENDAR" >/dev/null
        return 0
    fi

    labels="$(printf '%s\n' "$rows" | awk -F'\t' '{print $1}')"
    choice="$(printf '%s\n' "$labels" | rofi_pick "Prossimi 30 giorni")"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    [[ -n "$selected" ]] && calendar_event_detail_menu "$selected"
}

calendar_delete_event() {
    local item id tmp
    ensure_events_file
    item="$(
        jq -r 'sort_by(.date, .start)[] | "\(.date) \(.start)  \(.title)\t\(.id)"' "$EVENTS_FILE" |
            rofi_pick "Elimina evento"
    )"
    id="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$id" ]] || return 0

    tmp="$(mktemp)"
    jq --arg id "$id" 'map(select(.id != $id))' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"
    notify "Evento locale eliminato"
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
    cal_body="$(echo "$cal_body" | sed "s/\b${day_num}\b/<b><span color='${c_accent}'>${day_num}<\/span><\/b>/")"
    
    # Format headers cleanly
    month_view="<b><span color='${c_accent}'>${cal_header}</span></b>\n<span color='${c_muted}'>${cal_weekdays}</span>\n${cal_body}"

    # Get events for today
    events="$(
        calendar_events_for_date "$today" |
            awk -F'\t' '
                NF {
                    time_str = ($8 == "true" || $1 == "") ? "Tutto il giorno" : $1
                    print time_str "\t" $3
                }
            ' |
            sed -n '1,5p'
    )"
    
    local events_formatted
    if [[ -z "$events" ]]; then
        events_formatted="<span color='${c_muted}'><i>Nessun impegno</i></span>"
    else
        events_formatted=""
        while IFS=$'\t' read -r ev_time ev_title; do
            if [[ -n "$ev_title" ]]; then
                # Clean event rendering: "12:00  Meeting Name"
                if [[ "$ev_time" == "Tutto il giorno" ]]; then
                    events_formatted="${events_formatted}<span color='${c_muted}'>all-day</span>  ${ev_title}\n"
                else
                    events_formatted="${events_formatted}<span color='${c_yellow}'>${ev_time}</span>  ${ev_title}\n"
                fi
            fi
        done <<< "$events"
    fi
    
    # Reassemble a beautifully clean, minimalist card
    printf '%b\n\n<b><span color="%s">IMPEGNI DI OGGI</span></b>\n%b' \
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
            printf '%s\n' \
                "󰑓 Sincronizza Google Calendar" \
                " Aggiungi evento locale" \
                "󰃭 Eventi oggi" \
                "󰔚 Prossimi 30 giorni" \
                "󰥔 Scegli data" \
                "󰧭 Elimina evento locale" \
                "󰖟 Apri Google Calendar" \
                "󰒓 Config sync" \
                "󰌍 Indietro" |
                rofi_pick_msg "$month" "$message" "$THEME_CALENDAR"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "󰑓 Sincronizza Google Calendar") calendar_sync_google ;;
            " Aggiungi evento locale") calendar_add_event ;;
            "󰃭 Eventi oggi") calendar_show_date "$today" ;;
            "󰔚 Prossimi 30 giorni") calendar_show_upcoming ;;
            "󰥔 Scegli data")
                picked_date="$(rofi_input "Data (YYYY-MM-DD)" "$today")"
                [[ -n "$picked_date" ]] && calendar_show_date "$(date -d "$picked_date" +%F 2>/dev/null || printf '%s' "$today")"
                ;;
            "󰧭 Elimina evento locale") calendar_delete_event ;;
            "󰖟 Apri Google Calendar") xdg-open "https://calendar.google.com/calendar/u/0/r" >/dev/null 2>&1 & ;;
            "󰒓 Config sync") "$REMOTE_SYNC_SCRIPT" config ;;
            "󰌍 Indietro")
                back_or_main
                return 0
                ;;
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
        choice="$(
            printf '%s\n' \
                "$lock" \
                "$logout" \
                "$suspend" \
                "$reboot" \
                "$poweroff" \
                "$back" |
                rofi_pick_msg "$title" "Hyprland\n$(system_text "Select an action")"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
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
            "$back")
                back_or_main
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
        local bluetooth wifi audio brightness battery keyboard notifications calendar wallpapers display widgets power floating
        bluetooth="$(menu_item "󰂯" "Bluetooth")"
        wifi="$(menu_item "󰤨" "Wi-Fi")"
        audio="$(menu_item "󰓃" "Sound")"
        brightness="󰃠 Luminosità"
        battery="$(menu_item "󰂎" "Battery")"
        keyboard="$(menu_item "" "Keyboard")"
        notifications="$(menu_item "󰵙" "Notifications")"
        calendar="$(menu_item "󰃭" "Calendar")"
        wallpapers="$(menu_item "󰸉" "Wallpaper")"
        display="󰍹 Proietta schermo"
        widgets="󱓞 Widget desktop"
        floating="󱂬 Floating Manager"
        power="$(menu_item "󰐥" "Power Off")"

        choice="$(
            printf '%s\n' \
                "$bluetooth" \
                "$wifi" \
                "$audio" \
                "$brightness" \
                "$battery" \
                "$keyboard" \
                "$notifications" \
                "$calendar" \
                "$wallpapers" \
                "$display" \
                "$widgets" \
                "$floating" \
                "$power" |
                rofi_pick "$(system_text "Settings")"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$bluetooth") MENU_STATE="bluetooth"; return 0 ;;
            "$wifi") MENU_STATE="wifi"; return 0 ;;
            "$audio") MENU_STATE="audio"; return 0 ;;
            "$brightness") "$HOME/.config/anto426/brightness_menu.sh"; return 0 ;;
            "$battery") MENU_STATE="battery"; return 0 ;;
            "$keyboard") MENU_STATE="keyboard"; return 0 ;;
            "$notifications") MENU_STATE="notifications"; return 0 ;;
            "$calendar") MENU_STATE="calendar"; return 0 ;;
            "$wallpapers") "$HOME/.config/anto426/wallpaper_select.sh"; return 0 ;;
            "$display") "$HOME/.config/anto426/projection_menu.sh"; return 0 ;;
            "$widgets") "$HOME/.config/anto426/widgets.sh" toggle; return 0 ;;
            "$floating") "$HOME/.config/anto426/floating_manager.sh" menu; return 0 ;;
            "$power") MENU_STATE="power"; return 0 ;;
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
