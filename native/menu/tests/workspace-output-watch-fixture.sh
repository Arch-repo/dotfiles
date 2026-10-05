#!/usr/bin/env bash
set -euo pipefail

menu_binary="${1:?percorso anto-menu mancante}"
project="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
repo_root="$(cd -- "$project/../.." && pwd -P)"
temporary="$(mktemp -d)"

cleanup() {
    rm -rf -- "$temporary"
}
trap cleanup EXIT

runtime="$temporary/runtime"
old_signature="fixture-old"
new_signature="fixture-current"
old_socket="$runtime/hypr/$old_signature/.socket2.sock"
new_socket="$runtime/hypr/$new_signature/.socket2.sock"
mkdir -p -- "$(dirname -- "$old_socket")" "$(dirname -- "$new_socket")"
touch -- "$old_socket"
sleep 0.02
touch -- "$new_socket"

monitors='[
  {"name":"eDP-1","disabled":false,"mirrorOf":"none","focused":true},
  {"name":"HDMI-A-1","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"DP-2","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"ANTO-VIRTUAL-4","disabled":false,"mirrorOf":"none","focused":false}
]'
output="$temporary/watcher.out"
HYPRLAND_INSTANCE_SIGNATURE="$old_signature" \
    XDG_RUNTIME_DIR="$runtime" \
    ANTO_LOCAL_CONFIG_ROOT="$temporary/local" \
    ANTO_WORKSPACE_MONITORS_JSON="$monitors" \
    ANTO_WORKSPACE_DRY_RUN=1 \
    ANTO_WORKSPACE_WATCH_ONCE=1 \
    "$menu_binary" workspace-output watch >"$output" 2>&1

rules="$temporary/local/hypr/workspaces.conf"
grep -Fq 'workspace = 1, monitor:eDP-1, defaultName:eDP·1, persistent:true, default:true' "$rules"
grep -Fq 'workspace = 11, monitor:HDMI-A-1, defaultName:HDMI·1, persistent:true, default:true' "$rules"
grep -Fq 'workspace = 21, monitor:DP-2, defaultName:DP·1, persistent:true, default:true' "$rules"
grep -Fq 'workspace = 31, monitor:ANTO-VIRTUAL-4, defaultName:VIRT·1, persistent:true, default:true' "$rules"
[[ "$(grep -c '^DRY-RUN' "$output")" -eq 40 ]]
for slot in 6 7 8 9 10; do
    grep -Fqx "workspace = $slot, monitor:eDP-1, defaultName:eDP·$slot, persistent:false" "$rules"
    grep -Fq "defaultName:HDMI·$slot, persistent:false" "$rules"
    grep -Fq "defaultName:DP·$slot, persistent:false" "$rules"
    grep -Fq "defaultName:VIRT·$slot, persistent:false" "$rules"
done
[[ "$(grep -c 'persistent:true' "$rules")" -eq 20 ]]
grep -Fq $'WATCH-SESSION\tfixture-current' "$output"

# Connector suffixes are added only when two outputs share a family.  This is
# the same naming contract used by Super+0…9 and their Shift variants for
# physical and virtual outputs alike.
multi_family_monitors='[
  {"name":"eDP-1","disabled":false,"mirrorOf":"none","focused":true},
  {"name":"eDP-2","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"HDMI-A-1","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"HDMI-A-3","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"DP-1","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"DP-4","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"ANTO-VIRTUAL-2","disabled":false,"mirrorOf":"none","focused":false},
  {"name":"ANTO-VIRTUAL-5","disabled":false,"mirrorOf":"none","focused":false}
]'
listing="$(
    ANTO_WORKSPACE_MONITORS_JSON="$multi_family_monitors" \
        "$menu_binary" workspace-output list
)"
for mapping in \
    $'eDP-1\teDP1\t1\teDP1·1' \
    $'eDP-2\teDP2\t1\teDP2·1' \
    $'HDMI-A-1\tHDMI1\t1\tHDMI1·1' \
    $'HDMI-A-3\tHDMI3\t1\tHDMI3·1' \
    $'DP-1\tDP1\t1\tDP1·1' \
    $'DP-4\tDP4\t1\tDP4·1' \
    $'ANTO-VIRTUAL-2\tVIRT2\t1\tVIRT2·1' \
    $'ANTO-VIRTUAL-5\tVIRT5\t1\tVIRT5·1'; do
    grep -Fqx "$mapping" <<<"$listing"
done

unit="$repo_root/.config/systemd/user/anto426-workspace-output-watch.service"
launcher="$repo_root/support/compat/workspace_output_watch_start.sh"
autostart="$repo_root/.config/hypr/conf/autostart.conf"
grep -Fq 'Restart=on-failure' "$unit"
grep -Fq 'systemctl --user import-environment' "$launcher"
grep -Fq 'workspace_output_watch_start.sh' "$autostart"
if grep -Eq '^exec-once = .*anto-menu workspace-output watch' "$autostart"; then
    printf 'Il watcher diretto è ancora presente in autostart\n' >&2
    exit 1
fi

# Cycle next/previous respects focused monitor and clamps cleanly.
next_out="$(
    ANTO_WORKSPACE_DRY_RUN=1 \
    ANTO_WORKSPACE_MONITORS_JSON='[{"name":"eDP-1","disabled":false,"mirrorOf":"none","focused":true,"activeWorkspace":{"id":-1337,"name":"eDP·1"}}]' \
    "$menu_binary" workspace-output next
)"
grep -Fq $'dispatch\tworkspace\t2' <<<"$next_out"

prev_out="$(
    ANTO_WORKSPACE_DRY_RUN=1 \
    ANTO_WORKSPACE_MONITORS_JSON='[{"name":"eDP-1","disabled":false,"mirrorOf":"none","focused":true,"activeWorkspace":{"id":-1338,"name":"eDP·2"}}]' \
    "$menu_binary" workspace-output previous
)"
grep -Fq $'dispatch\tworkspace\t1' <<<"$prev_out"

printf 'workspace output watcher fixture: ok\n'
