#!/usr/bin/env bash
set -u

tool="${0##*/}"
arguments="$*"

case "$tool" in
    nmcli)
        case "$arguments" in
            *"-f WIFI general"*) printf 'enabled\n' ;;
            *"-f CONNECTIVITY general"*) printf 'full\n' ;;
            *"-f NAME,TYPE connection show"*)
                printf 'Casa\\:Lab:802-11-wireless\nEthernet:802-3-ethernet\n'
                ;;
            *"-f DEVICE,TYPE,STATE,CONNECTION device status"*)
                printf 'wlan0:wifi:connected:Casa\\:Lab\neth0:ethernet:connected:LAN\n'
                ;;
            *"-f TYPE,STATE,CONNECTION device status"*)
                printf 'wifi:connected:Casa\\:Lab\nethernet:connected:LAN\n'
                ;;
            *"-g IP4.ADDRESS device show"*) printf '192.0.2.10/24\n' ;;
            *"-g IP4.GATEWAY device show"*) printf '192.0.2.1\n' ;;
            *"-g IP4.DNS device show"*) printf '192.0.2.53\n1.1.1.1\n' ;;
            *"IN-USE,SSID,SIGNAL,SECURITY,BSSID,FREQ,CHAN"*)
                printf '*:Casa\\:Lab:88:WPA2:AA\\:BB\\:CC\\:DD\\:EE\\:01:5180:36\n'
                printf ':Ospiti:54:--:AA\\:BB\\:CC\\:DD\\:EE\\:02:2412:1\n'
                ;;
            *) exit 0 ;;
        esac
        ;;
    bluetoothctl)
        case "$arguments" in
            list)
                printf 'Controller AA:BB:CC:DD:EE:FF Mock Adapter [default]\n'
                ;;
            "show AA:BB:CC:DD:EE:FF"|show)
                cat <<'EOF'
Controller AA:BB:CC:DD:EE:FF
        Name: Mock Adapter
        Alias: Studio
        Powered: yes
        Pairable: yes
        Discoverable: no
        Discovering: no
EOF
                ;;
            "devices Paired")
                printf 'Device 11:22:33:44:55:66 Mock Headset\n'
                ;;
            "devices Connected")
                printf 'Device 11:22:33:44:55:66 Mock Headset\n'
                ;;
            devices)
                printf 'Device 11:22:33:44:55:66 Mock Headset\n'
                ;;
            "info 11:22:33:44:55:66")
                cat <<'EOF'
Device 11:22:33:44:55:66
        Name: Mock Headset
        Alias: Cuffie Test
        Icon: audio-headset
        Paired: yes
        Trusted: yes
        Blocked: no
        Connected: yes
        Battery Percentage: 0x55 (85)
        RSSI: -42
EOF
                ;;
            *) exit 0 ;;
        esac
        ;;
    pactl)
        case "$arguments" in
            *"list cards")
                cat <<'EOF'
[{"name":"bluez_card.11_22_33_44_55_66","properties":{"device.string":"11:22:33:44:55:66"},"active_profile":"a2dp-sink","profiles":{"a2dp-sink":{"available":"yes","description":"Stereo alta qualità"},"headset-head-unit":{"available":"yes","description":"Cuffie e microfono"}}}]
EOF
                ;;
            *"list sinks")
                printf '[{"name":"bluez_output.11_22_33_44_55_66.1","properties":{"device.string":"11:22:33:44:55:66"}}]\n'
                ;;
            *"list sources")
                printf '[{"name":"bluez_input.11_22_33_44_55_66.0","properties":{"device.string":"11:22:33:44:55:66"}}]\n'
                ;;
            *) exit 0 ;;
        esac
        ;;
    wpctl)
        case "$arguments" in
            "get-volume @DEFAULT_AUDIO_SINK@") printf 'Volume: 0.73\n' ;;
            "get-volume @DEFAULT_AUDIO_SOURCE@") printf 'Volume: 0.42\n' ;;
            "inspect @DEFAULT_AUDIO_SINK@")
                printf '  node.description = "Mock Speakers"\n'
                ;;
            *) exit 0 ;;
        esac
        ;;
    playerctl)
        case "$arguments" in
            "metadata --format {{playerName}}") printf 'Mock Player\n' ;;
            status) printf 'Playing\n' ;;
            "metadata artist") printf 'Artista\n' ;;
            "metadata title") printf 'Titolo\n' ;;
            "metadata album") printf 'Album\n' ;;
            *) exit 0 ;;
        esac
        ;;
    brightnessctl)
        printf 'mock,backlight,500,73%%,1000\n'
        ;;
    powerprofilesctl)
        case "$arguments" in
            get) printf 'balanced\n' ;;
            list) printf '* balanced:\n  performance:\n' ;;
            *) exit 0 ;;
        esac
        ;;
    hyprctl)
        case "$arguments" in
            "-j monitors"|"monitors -j")
                printf '[{"name":"eDP-1","focused":true}]\n'
                ;;
            "activewindow -j")
                printf '{"at":[10,20],"size":[800,600]}\n'
                ;;
            *) exit 0 ;;
        esac
        ;;
    notify-send|hyprshot|slurp|grim|wl-copy|tesseract|wf-recorder|xdg-open|pavucontrol|hyprlock|systemctl)
        exit 0
        ;;
    *)
        printf 'Mock sconosciuto: %s\n' "$tool" >&2
        exit 127
        ;;
esac
