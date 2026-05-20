#!/usr/bin/env bash
set -euo pipefail

session_dir="/usr/share/wayland-sessions"
session_file="$session_dir/hyprland.desktop"

if ((EUID != 0)); then
    exec sudo -- "$0" "$@"
fi

if [[ ! -x /usr/bin/start-hyprland ]]; then
    printf '[ERROR] /usr/bin/start-hyprland not found.\n' >&2
    exit 1
fi

install -d -m 755 "$session_dir"
cat >"$session_file" <<'EOF'
[Desktop Entry]
Name=Hyprland
Comment=An intelligent dynamic tiling Wayland compositor
Exec=/usr/bin/start-hyprland
TryExec=/usr/bin/start-hyprland
Type=Application
DesktopNames=Hyprland
Keywords=tiling;wayland;compositor;
EOF
chmod 644 "$session_file"

printf '[OK] Hyprland session now starts through start-hyprland.\n'
