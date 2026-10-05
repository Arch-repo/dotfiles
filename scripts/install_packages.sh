#!/usr/bin/env bash
# Runtime and build dependencies for the native desktop, on Arch Linux.
set -euo pipefail
packages=(hyprland gtk4 gtk4-layer-shell cmake ninja gcc pkgconf json-c wayland
    wayland-protocols python noto-fonts noto-fonts-emoji ttf-jetbrains-mono-nerd
    qt6-base qt6-declarative qt6-tools sddm hyprlock fprintd libfprint
    waybar swaync ghostty cava playerctl brightnessctl networkmanager bluez bluez-utils
    pipewire wireplumber pipewire-pulse alsa-utils imagemagick ffmpeg jq curl
    grim slurp wl-clipboard cliphist tesseract wf-recorder polkit-gnome)
sudo pacman -S --needed "${packages[@]}"
if command -v yay >/dev/null 2>&1; then
    yay -S --needed awww mpvpaper
elif command -v paru >/dev/null 2>&1; then
    paru -S --needed awww mpvpaper
else
    printf 'Install awww and mpvpaper from the AUR, then rerun scripts/install.py.\n' >&2
    exit 1
fi
