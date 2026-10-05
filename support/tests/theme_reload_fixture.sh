#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../compat" && pwd -P)"
reload_script="$script_dir/theme_reload.sh"
fixture="$(mktemp -d)"
trap 'rm -rf -- "$fixture"' EXIT

mkdir -p \
    "$fixture/bin" \
    "$fixture/config/anto426-local/theme" \
    "$fixture/config/anto426-local/hypr" \
    "$fixture/config/hypr/conf" \
    "$fixture/config/waybar" \
    "$fixture/state/anto426" \
    "$fixture/cache/awww"

wallpaper="$fixture/wallpaper.png"
other_wallpaper="$fixture/other.png"
: >"$wallpaper"
: >"$other_wallpaper"

printf '%s\n' '@define-color accent #db81aa;' \
    >"$fixture/config/anto426-local/theme/colors.css"
printf '%s\n' \
    '$anto426_active_border = rgba(db81aaee)' \
    '$anto426_inactive_border = rgba(342b35aa)' \
    '$anto426_shadow = rgba(00000055)' \
    >"$fixture/config/anto426-local/hypr/theme.generated.conf"
printf '%s\n' 'monitor=eDP-1,preferred,0x0,1' >"$fixture/config/hypr/conf/monitors.conf"
printf '%s\n' '@import "../anto426-local/theme/colors.css";' \
    >"$fixture/config/waybar/style.css"
printf '%s\n' "$wallpaper" >"$fixture/cache/awww/current-wallpaper.path"
monitor_signature="$(sha256sum "$fixture/config/hypr/conf/monitors.conf")"

for command_name in hyprctl swaync-client pkill; do
    printf '%s\n' '#!/usr/bin/env bash' 'exit 0' >"$fixture/bin/$command_name"
    chmod 755 "$fixture/bin/$command_name"
done

printf '%s\n' \
    '#!/usr/bin/env bash' \
    'if [[ "$*" == "-x waybar" ]]; then exit "${FAKE_WAYBAR_PGREP_STATUS:-0}"; fi' \
    'if [[ "$*" == "-x ghostty" ]]; then exit 0; fi' \
    'exit 1' \
    >"$fixture/bin/pgrep"
chmod 755 "$fixture/bin/pgrep"

printf '%s\n' \
    '#!/usr/bin/env bash' \
    'exit 0' \
    >"$fixture/bin/timeout"
chmod 755 "$fixture/bin/timeout"

printf '%s\n' \
    '#!/usr/bin/env bash' \
    'if [[ "$*" == *"is-active waybar.service"* ]]; then exit "${FAKE_WAYBAR_SERVICE_STATUS:-0}"; fi' \
    'if [[ "$*" == *"is-active app-com.mitchellh.ghostty.service"* ]]; then exit 0; fi' \
    'exit 0' \
    >"$fixture/bin/systemctl"
chmod 755 "$fixture/bin/systemctl"

export HOME="$fixture/home"
export XDG_CONFIG_HOME="$fixture/config"
export ANTO_LOCAL_CONFIG_ROOT="$fixture/config/anto426-local"
export XDG_STATE_HOME="$fixture/state"
export XDG_CACHE_HOME="$fixture/cache"
export PATH="$fixture/bin:/usr/bin:/bin"
export ANTO426_THEME_RELOAD_DEBOUNCE_SECONDS=0
export ANTO426_THEME_RELOAD_TRACE="$fixture/trace"

"$reload_script" "$wallpaper"
[[ "$(wc -l <"$fixture/trace")" -eq 4 ]]
grep -q '^hyprctl --batch keyword general:col.active_border' "$fixture/trace"
grep -q '^systemctl --user kill --signal=SIGUSR2 --kill-whom=main waybar.service$' "$fixture/trace"
grep -q '^timeout --foreground 3 swaync-client -rs$' "$fixture/trace"
grep -q '^systemctl --user reload app-com.mitchellh.ghostty.service$' "$fixture/trace"
[[ "$(sha256sum "$fixture/config/hypr/conf/monitors.conf")" == "$monitor_signature" ]]
[[ "$(sed -n '1p' "$fixture/cache/awww/current-wallpaper.path")" == "$wallpaper" ]]

# An unmanaged Waybar receives the same official signal through an exact
# process-name match.  This must never broaden to custom module descendants.
printf '%s\n' '@define-color accent #8fc7ff;' \
    >"$fixture/config/anto426-local/theme/colors.css"
rm -f "$fixture/trace"
FAKE_WAYBAR_SERVICE_STATUS=1 "$reload_script" "$wallpaper"
grep -q '^pkill -SIGUSR2 -x waybar$' "$fixture/trace"
if grep -q '^touch .*waybar/style.css$' "$fixture/trace"; then
    printf 'Waybar CSS watcher fallback ran despite a live exact process match\n' >&2
    exit 1
fi

# An identical palette produces no second reload wave.
rm -f "$fixture/trace"
"$reload_script" "$wallpaper"
[[ ! -e "$fixture/trace" ]]

# Two simultaneous commits serialize; only one reaches desktop consumers.
rm -f "$fixture/state/anto426/theme-reload.signature" "$fixture/trace"
"$reload_script" "$wallpaper" &
first_pid=$!
"$reload_script" "$wallpaper" &
second_pid=$!
wait "$first_pid" "$second_pid"
[[ "$(wc -l <"$fixture/trace")" -eq 4 ]]

# A superseded wallpaper job may not touch any desktop component.
rm -f "$fixture/state/anto426/theme-reload.signature" "$fixture/trace"
printf '%s\n' "$other_wallpaper" >"$fixture/cache/awww/current-wallpaper.path"
"$reload_script" "$wallpaper"
[[ ! -e "$fixture/trace" ]]

if grep -Eq 'hyprctl reload|systemctl --user restart waybar|xdg-desktop-portal|widgets\.sh reload' \
    "$fixture/state/anto426/theme-reload.log" "$fixture/trace" 2>/dev/null; then
    printf 'A forbidden full-session reload escaped the fixture\n' >&2
    exit 1
fi

printf 'theme reload fixture: ok\n'
