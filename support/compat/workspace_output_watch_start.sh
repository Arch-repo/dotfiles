#!/usr/bin/env bash
set -euo pipefail

# A user service inherits the user manager's environment, not Hyprland's
# process environment.  Import the values from this exact compositor instance
# before starting the watcher, so hyprctl and the event socket agree.
session_variables=(
    WAYLAND_DISPLAY
    HYPRLAND_INSTANCE_SIGNATURE
    XDG_CURRENT_DESKTOP
    XDG_SESSION_DESKTOP
    XDG_SESSION_TYPE
)
available=()
for variable in "${session_variables[@]}"; do
    if [[ -n "${!variable:-}" ]]; then
        available+=("$variable")
    fi
done

if (( ${#available[@]} )); then
    systemctl --user import-environment "${available[@]}"
    if command -v dbus-update-activation-environment >/dev/null 2>&1; then
        dbus-update-activation-environment --systemd "${available[@]}" || true
    fi
fi

systemctl --user start --no-block anto426-workspace-output-watch.service
