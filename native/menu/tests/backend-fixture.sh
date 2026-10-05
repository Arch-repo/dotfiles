#!/usr/bin/env bash
set -euo pipefail

backend="${1:?percorso backend mancante}"
project="$(cd "$(dirname "$0")/.." && pwd)"
fixtures="$project/tests/fixtures"
temporary="$(mktemp -d)"
trap 'rm -rf -- "$temporary"' EXIT
mock="$temporary/bin"
mkdir -p "$mock"
for tool in nmcli bluetoothctl pactl wpctl playerctl brightnessctl \
    powerprofilesctl hyprctl notify-send hyprshot slurp grim wl-copy \
    tesseract wf-recorder xdg-open pavucontrol hyprlock systemctl; do
    ln -s "$fixtures/mock-tools.sh" "$mock/$tool"
done

export ANTO_MENU_NMCLI="$mock/nmcli"
export ANTO_MENU_BLUETOOTHCTL="$mock/bluetoothctl"
export ANTO_MENU_PACTL="$mock/pactl"
export ANTO_MENU_WPCTL="$mock/wpctl"
export ANTO_MENU_PLAYERCTL="$mock/playerctl"
export ANTO_MENU_BRIGHTNESSCTL="$mock/brightnessctl"
export ANTO_MENU_POWERPROFILESCTL="$mock/powerprofilesctl"
export ANTO_MENU_HYPRCTL="$mock/hyprctl"
export ANTO_MENU_NOTIFY_SEND="$mock/notify-send"
export ANTO_MENU_NO_NOTIFY=1
export ANTO_MENU_POWER_SUPPLY_ROOT="$fixtures/power"
export ANTO_MENU_THERMAL_ROOT="$fixtures/thermal"
export ANTO_MENU_PROC_ROOT="$fixtures/proc"
export ANTO_LOCAL_CONFIG_ROOT="$temporary/local"
export ANTO_MENU_NOTES_LEGACY_CONFIG="$fixtures/notes-legacy.env"
anto_config="$mock/anto-config"
ln -s "$backend" "$anto_config"
default_root="$(env -u ANTO_LOCAL_CONFIG_ROOT \
    XDG_CONFIG_HOME="$temporary/xdg-config" "$anto_config" root)"
[[ "$default_root" == "$temporary/xdg-config/anto426-local" ]]
fallback_root="$(env -u ANTO_LOCAL_CONFIG_ROOT -u XDG_CONFIG_HOME \
    HOME="$temporary/home" "$anto_config" root)"
[[ "$fallback_root" == "$temporary/home/.config/anto426-local" ]]

require_text() {
    local output="$1" wanted="$2"
    if [[ "$output" != *"$wanted"* ]]; then
        printf 'Output mancante: %s\n---\n%s\n' "$wanted" "$output" >&2
        exit 1
    fi
}

network="$("$backend" network snapshot)"
require_text "$network" $'STATUS\ttrue\tenabled\tconnected\twlan0\tCasa:Lab'
require_text "$network" $'NETWORK\tCasa:Lab\t88\tWPA2\ttrue\ttrue'
require_text "$network" $'NETWORK\tOspiti\t54\t--\tfalse\tfalse'

bluetooth="$("$backend" bluetooth status)"
require_text "$bluetooth" $'STATUS\tyes\tAA:BB:CC:DD:EE:FF\tStudio\tyes'
devices="$("$backend" bluetooth devices all)"
require_text "$devices" $'DEVICE\t11:22:33:44:55:66\tMock Headset\tCuffie Test'
require_text "$devices" $'\tyes\tyes\tno\tyes\t85\t-42'
profiles="$("$backend" bluetooth audio-profiles 11:22:33:44:55:66)"
require_text "$profiles" $'AUDIO_PROFILE\tbluez_card.11_22_33_44_55_66\ta2dp-sink\tyes\tyes'

audio="$("$backend" audio snapshot)"
require_text "$audio" $'SINK\tVolume: 0.73'
require_text "$audio" $'NAME\tMock Speakers'
require_text "$audio" $'MPRIS\tMock Player\tPlaying\tArtista\tTitolo\tAlbum'

energy="$("$backend" energy snapshot)"
require_text "$energy" $'brightness\t73'
require_text "$energy" $'capacity\t87'
require_text "$energy" $'profile\tbalanced'

system="$(G_DEBUG=fatal-criticals "$backend" system snapshot)"
require_text "$system" $'volume\t73%'
require_text "$system" $'network\tCasa:Lab'
require_text "$system" $'displays\t1 attivo'
require_text "$system" $'bluetooth\t1 connesso'
require_text "$system" $'battery\t87%'

# Real commands often return empty stdout/stderr. Empty GBytes must remain
# valid strings, and a missing backlight must not index past its field list.
empty_system="$(G_DEBUG=fatal-criticals \
    ANTO_MENU_WPCTL=/usr/bin/true ANTO_MENU_NMCLI=/usr/bin/true \
    ANTO_MENU_BRIGHTNESSCTL=/usr/bin/true \
    ANTO_MENU_HYPRCTL=/usr/bin/true ANTO_MENU_BLUETOOTHCTL=/usr/bin/true \
    "$backend" system snapshot)"
require_text "$empty_system" $'volume\tn/d'
require_text "$empty_system" $'network\tDisconnessa'
require_text "$empty_system" $'brightness\tn/d'
require_text "$empty_system" $'displays\tn/d'
require_text "$empty_system" $'bluetooth\tSpento'
require_text "$empty_system" $'battery\t87%'

hardware="$("$backend" system hardware-snapshot)"
require_text "$hardware" $'CPU\t0.30'
require_text "$hardware" $'TEMP\t51°C'
require_text "$hardware" $'PROFILE\tbalanced'

dry="$(ANTO_MENU_DRY_RUN=1 "$backend" network connect-open 'rete; non codice')"
require_text "$dry" $'DRYRUN\tnetwork\tconnect-open\trete; non codice'
dry="$(ANTO_MENU_DRY_RUN=1 "$backend" bluetooth connect 11:22:33:44:55:66)"
require_text "$dry" $'DRYRUN\tconnect\t11:22:33:44:55:66'
dry="$(ANTO_MENU_DRY_RUN=1 "$backend" session reboot)"
require_text "$dry" $'DRYRUN\tsession\treboot'
dry="$(ANTO_MENU_DRY_RUN=1 ANTO_MENU_SCREENSHOT_DIR="$temporary/not-created/screens" \
    "$backend" capture area)"
require_text "$dry" $'DRYRUN\tcapture\tregion'
[[ ! -e "$temporary/not-created" ]]
dry="$(ANTO_MENU_DRY_RUN=1 "$backend" record monitor-audio)"
require_text "$dry" $'DRYRUN\trecord\tmonitor\taudio'
dry="$(ANTO_MENU_DRY_RUN=1 ANTO426_NOTES_DIR="$temporary/notes-not-created" \
    "$backend" notes open)"
require_text "$dry" $'DRYRUN\tnotes\topen'
require_text "$dry" 'mock-editor'
[[ ! -e "$temporary/notes-not-created" ]]
[[ ! -e "$temporary/local/notes" ]]

root="$("$anto_config" root)"
[[ "$root" == "$temporary/local" ]]
path="$("$anto_config" get display/layout.json)"
[[ "$path" == "$temporary/local/display/layout.json" ]]
missing_status="$("$anto_config" status)"
require_text "$missing_status" $'exists\tfalse'
require_text "$missing_status" $'files\t0'
[[ ! -e "$temporary/local" ]]
missing_tree="$("$anto_config" tree)"
require_text "$missing_tree" $'ROOT\t'
require_text "$missing_tree" 'MISSING'
if "$anto_config" get ../escape >/dev/null 2>&1; then
    printf 'Il percorso ../escape è stato accettato\n' >&2
    exit 1
fi
migrated="$("$anto_config" migrate \
    "$fixtures/migrate-source.txt" tests/imported.txt)"
[[ "$migrated" == "$temporary/local/tests/imported.txt" ]]
cmp "$fixtures/migrate-source.txt" "$migrated"
notes_config="$("$backend" notes config-path)"
[[ "$notes_config" == "$temporary/local/notes/notes.env" ]]
cmp "$fixtures/notes-legacy.env" "$notes_config"
mkdir -p "$temporary/outside"
printf 'non attraversare\n' >"$temporary/outside/secret.txt"
ln -s "$temporary/outside" "$temporary/local/external-link"
status="$("$anto_config" status)"
require_text "$status" $'exists\ttrue'
require_text "$status" $'links\t1'
file_count="$(awk -F'\t' '$1 == "files" {print $2}' <<<"$status")"
[[ "$file_count" =~ ^[0-9]+$ && "$file_count" -ge 2 ]]
tree="$("$anto_config" tree)"
require_text "$tree" $'DIR\tnotes'
require_text "$tree" $'FILE\tnotes/notes.env'
require_text "$tree" $'DIR\ttests'
require_text "$tree" $'FILE\ttests/imported.txt'
require_text "$tree" $'LINK\texternal-link'
if [[ "$tree" == *"secret.txt"* ]]; then
    printf 'anto-config tree ha seguito un link simbolico\n' >&2
    exit 1
fi
[[ "$("$backend" config root)" == "$root" ]]

printf 'backend fixtures: ok\n'
