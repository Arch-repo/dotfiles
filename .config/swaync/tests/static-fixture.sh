#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
swaync_dir="$(cd -- "$script_dir/.." && pwd)"
repo_root="$(cd -- "$swaync_dir/../.." && pwd)"
config="$swaync_dir/config.json"
style="$repo_root/build/design/swaync.css"
waybar="$repo_root/.config/waybar/config"
fixture="$(mktemp -d)"
trap 'rm -rf -- "$fixture"' EXIT

jq -e '
    .["control-center-width"] >= 300 and
    .["control-center-height"] > 0 and
    .["control-center-margin-top"] >= 14 and
    .["fit-to-screen"] == false and
    .["ignore-gtk-theme"] == true and
    .widgets == ["title", "dnd", "notifications"] and
    .["widget-config"].title["clear-all-button"] == true and
    .["widget-config"].notifications.vexpand == true
' "$config" >/dev/null

jq -e '
    .["custom/notifications"].exec == "swaync-client -swb" and
    .["custom/notifications"].format == "{icon}" and
    .["custom/notifications"]["on-click"] == "swaync-client -t -sw" and
    .["custom/notifications"]["on-click-right"] == "swaync-client -d -sw" and
    .["custom/notifications"]["on-click-middle"] == "$HOME/.local/bin/anto-menu notifications"
' "$waybar" >/dev/null

rg -F '@import url("../../../.config/anto426-local/theme/colors.css");' "$style" >/dev/null
rg -F '.control-center scrollbar slider,' "$style" >/dev/null
rg -F '.notification-row .notification-background .notification .notification-default-action .notification-content' "$style" >/dev/null
rg -F 'padding: 14px 0 0;' "$style" >/dev/null

if command -v cc >/dev/null 2>&1 && pkg-config --exists gtk4; then
    mkdir -p "$fixture/.local/share/anto-desktop" "$fixture/.config/anto426-local/theme"
    cp -- "$style" "$fixture/.local/share/anto-desktop/style.css"
    cp -- "$repo_root/native/menu/assets/tokens.css" \
        "$fixture/.config/anto426-local/theme/colors.css"
    cc -Wall -Wextra -Werror "$script_dir/css_parse.c" \
        -o "$fixture/css-parse" $(pkg-config --cflags --libs gtk4)
    css_errors="$($fixture/css-parse "$fixture/.local/share/anto-desktop/style.css" 2>&1)"
    if [[ -n "$css_errors" ]]; then
        printf '%s\n' "$css_errors" >&2
        exit 1
    fi
fi

printf 'swaync static fixture: ok\n'
