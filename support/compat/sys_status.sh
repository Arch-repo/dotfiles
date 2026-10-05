#!/usr/bin/env bash

source "$HOME/.config/anto426/lib/bluetooth_audio.sh" 2>/dev/null || true

# Keep track of active text
current_text=""

TOOLTIP="Stato Sistema: Click sinistro per Pannello Controlli, Click destro per Notifiche, Click centrale per DND"

pango_escape() {
    local value="$1"
    value="${value//&/&amp;}"
    value="${value//</&lt;}"
    value="${value//>/&gt;}"
    value="${value//\'/&apos;}"
    value="${value//\"/&quot;}"
    printf '%s' "$value"
}

emit_json() {
    local text="$1"

    if command -v jq >/dev/null 2>&1; then
        jq -cn --arg text "$text" --arg tooltip "$TOOLTIP" '{text: $text, tooltip: $tooltip}'
    else
        printf '{"text":"󰒓","tooltip":"jq non disponibile"}\n'
    fi
}

get_status_text() {
    local state="$1"
    
    # Load dynamic colors
    colors_file="${ANTO_LOCAL_CONFIG_ROOT:-${XDG_CONFIG_HOME:-$HOME/.config}/anto426-local}/theme/colors.sh"
    if [ -f "$colors_file" ]; then
        source "$colors_file"
    fi
    local accent_color="${ANTO426_ACCENT:-#db81aa}"
    
    # 1. Notifications
    local noti_count
    noti_count=$(swaync-client -c 2>/dev/null || echo 0)
    case "$noti_count" in ""|*[!0-9]*) noti_count=0 ;; esac
    if [ "$noti_count" -gt 0 ]; then
        noti_text="󰂚 <span color='$accent_color'>$noti_count</span>"
    else
        noti_text="󰂜"
    fi

    # 2. State info
    local info_text=""
    if [ "$state" -eq 0 ]; then
        # Audio & Battery
        local vol
        vol=$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | awk '{print int($2*100)}')
        vol=${vol:-50}
        local is_muted
        is_muted=$(wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null | grep -q "MUTED" && echo "yes" || echo "no")
        local default_sink audio_icon
        default_sink="$(pactl info 2>/dev/null | awk -F': ' '/Default Sink:/ {print $2; exit}')"
        audio_icon=""
        [[ "$default_sink" == bluez_output.* ]] && audio_icon=""
        if [ "$is_muted" = "yes" ]; then
            vol_text=" Mute"
        else
            vol_text="$audio_icon ${vol}%"
        fi
        
        local bat_cap=100
        local bat_stat="Unknown"
        if [ -d /sys/class/power_supply/BAT0 ]; then
            bat_cap=$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo 100)
            bat_stat=$(cat /sys/class/power_supply/BAT0/status 2>/dev/null || echo "Unknown")
        elif [ -d /sys/class/power_supply/BAT1 ]; then
            bat_cap=$(cat /sys/class/power_supply/BAT1/capacity 2>/dev/null || echo 100)
            bat_stat=$(cat /sys/class/power_supply/BAT1/status 2>/dev/null || echo "Unknown")
        fi
        
        local bat_icon
        if [ "$bat_stat" = "Charging" ] || [ "$bat_stat" = "Full" ]; then
            bat_icon=""
        else
            if [ "$bat_cap" -gt 80 ]; then bat_icon="";
            elif [ "$bat_cap" -gt 50 ]; then bat_icon="";
            elif [ "$bat_cap" -gt 20 ]; then bat_icon="";
            else bat_icon=""; fi
        fi
        info_text="$vol_text  $bat_icon ${bat_cap}%"
        
    elif [ "$state" -eq 1 ]; then
        # Network & Bluetooth
        local wifi_ssid
        wifi_ssid=$(nmcli -t -f active,ssid dev wifi 2>/dev/null | grep '^yes:' | cut -d: -f2 | head -n1)
        if [ -n "$wifi_ssid" ]; then
            net_text="󰤨 $(pango_escape "$wifi_ssid")"
        else
            local eth_status
            eth_status=$(cat /sys/class/net/e*/operstate 2>/dev/null | head -n1)
            if [ "$eth_status" = "up" ]; then
                net_text="󰈀 Cablata"
            else
                net_text="󰤭 Offline"
            fi
        fi
        
        local bt_status="󰂲"
        if bluetoothctl show 2>/dev/null | grep -q "Powered: yes"; then
            local bt_audio
            bt_audio="$(bt_audio_connected_summary 2>/dev/null || true)"
            if [ -n "$bt_audio" ]; then
                bt_status=" $(pango_escape "$bt_audio")"
            else
                local devices
                devices=$(bluetoothctl devices Connected 2>/dev/null | wc -l)
                if [ "$devices" -gt 0 ]; then
                    bt_status="󰂱 Connected"
                else
                    bt_status="󰂯 On"
                fi
            fi
        fi
        info_text="$net_text  $bt_status"
        
    else
        # CPU & RAM
        local cpu
        cpu=$(grep 'cpu ' /proc/stat | awk '{usage=($2+$4)*100/($2+$4+$5)} END {printf "%d", usage}')
        cpu=${cpu:-0}
        
        local mem_used_pct
        mem_used_pct=$(free | grep Mem | awk '{printf "%d", $3/$2 * 100}')
        mem_used_pct=${mem_used_pct:-0}
        
        info_text="󰻠 ${cpu}%  󰍛 ${mem_used_pct}%"
    fi
    
    echo "$noti_text  |  $info_text"
}

state=0
# Loop forever
while true; do
    next_text=$(get_status_text "$state")
    
    if [ -z "$current_text" ]; then
        # First print
        emit_json "$next_text"
    else
        # Fade out current, fade in next
        # Fade out
        for op in "80%" "60%" "40%" "20%" "1%"; do
            emit_json "<span alpha='$op'>$current_text</span>"
            sleep 0.04
        done
        # Fade in
        for op in "20%" "40%" "60%" "80%" "100%"; do
            emit_json "<span alpha='$op'>$next_text</span>"
            sleep 0.04
        done
    fi
    
    current_text="$next_text"
    state=$(( (state + 1) % 3 ))
    
    # Sleep between status cycles
    sleep 4
done
