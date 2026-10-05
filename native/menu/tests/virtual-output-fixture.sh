#!/usr/bin/env bash
set -euo pipefail

menu_binary="${1:?percorso anto-menu mancante}"
project="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
fixture_root="$(mktemp -d)"

cleanup() {
    if [[ -n "${fixture_root:-}" && -d "$fixture_root" ]]; then
        rm -rf -- "$fixture_root"
    fi
}
trap cleanup EXIT

state_file="$fixture_root/virtual-monitors.json"
before_list="$fixture_root/state-before-list.json"

jq -n '{
    version: 1,
    outputs: [
        {
            name: "ANTO-VIRTUAL-1",
            width: 1920,
            height: 1080,
            refresh: 60,
            scale: 1,
            x: 2560,
            y: 0,
            transform: 0,
            persistent: true
        },
        {
            name: "ANTO-VIRTUAL-2",
            width: 1080,
            height: 1920,
            refresh: 60,
            scale: 1,
            x: 4480,
            y: 0,
            transform: 0,
            persistent: false
        }
    ]
}' >"$state_file"

export ANTO_VIRTUAL_STATE_FILE="$state_file"
export ANTO_VIRTUAL_DRY_RUN=1
export ANTO_VIRTUAL_HYPRCTL="$fixture_root/forbidden-hyprctl"
export ANTO_VIRTUAL_MONITORS_JSON='[
    {
        "name": "eDP-1",
        "disabled": false,
        "width": 2560,
        "height": 1600,
        "scale": 1.6,
        "x": 0,
        "y": 0
    },
    {
        "name": "ANTO-VIRTUAL-1",
        "disabled": false,
        "width": 1920,
        "height": 1080,
        "scale": 1,
        "x": 1600,
        "y": 0
    }
]'

cp -- "$state_file" "$before_list"
listing="$("$menu_binary" virtual-output list)"
rg -Fq $'ANTO-VIRTUAL-1\t1920\t1080\t60.000\t1.000\t2560\t0\t0\tpersistent\tonline' \
    <<<"$listing"
rg -Fq $'ANTO-VIRTUAL-2\t1080\t1920\t60.000\t1.000\t4480\t0\t0\ttemporary\toffline' \
    <<<"$listing"
cmp "$state_file" "$before_list"

"$menu_binary" virtual-output set-persistent \
    ANTO-VIRTUAL-2 persistent
jq -e '
    .outputs
    | map(select(.name == "ANTO-VIRTUAL-2" and .persistent == true))
    | length == 1
' "$state_file" >/dev/null

create_preset() {
    local width="$1"
    local height="$2"
    local output

    output="$(
        "$menu_binary" virtual-output create auto \
            "$width" "$height" 60 1 persistent
    )"
    rg -Fq $'DRY-RUN\t'"$ANTO_VIRTUAL_HYPRCTL"$'\toutput\tcreate\theadless' \
        <<<"$output"
}

create_preset 1920 1080
create_preset 1080 1920
create_preset 2560 1440

jq -e '
    [.outputs[]
     | select(.name | startswith("ANTO-VIRTUAL-"))
     | {width, height, refresh, scale, persistent}]
    | contains([
        {width: 1920, height: 1080, refresh: 60, scale: 1, persistent: true},
        {width: 1080, height: 1920, refresh: 60, scale: 1, persistent: true},
        {width: 2560, height: 1440, refresh: 60, scale: 1, persistent: true}
      ])
' "$state_file" >/dev/null

"$menu_binary" virtual-output remove ANTO-VIRTUAL-2
jq -e '
    all(.outputs[]; .name != "ANTO-VIRTUAL-2")
' "$state_file" >/dev/null

rg -Fq 'DISPLAY_SECTION_VIRTUAL' \
    "$project/src/modules/display"
rg -Fq '"MONITOR VIRTUALI"' \
    "$project/src/modules/display"
for preset in 1920x1080 1080x1920 2560x1440; do
    rg -Fq "\"$preset\"," \
        "$project/src/modules/display"
done

printf 'virtual output fixture: ok\n'
