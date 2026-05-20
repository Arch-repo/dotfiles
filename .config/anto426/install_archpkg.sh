#!/usr/bin/env bash
set -euo pipefail

RESET="\e[0m"
BOLD="\e[1m"
DIM="\e[2m"
PINK="\e[35m"
YELLOW="\e[33m"
GREEN="\e[32m"
BLUE="\e[34m"

ui_line() {
    printf '%b\n' "${PINK}---------------------------------------------------------------------${RESET}"
}

ui_banner() {
    printf '%b\n' "${PINK}${BOLD}"
    printf '  ANTO426 ARCH PACKAGE INSTALLER\n'
    printf '%b\n' "${RESET}${DIM}  Installs package dependencies used by the dotfiles.${RESET}"
    ui_line
}

ui_step() {
    local current="$1"
    local total="$2"
    local title="$3"

    printf '\n'
    ui_line
    printf '%b[%02d/%02d]%b %b%s%b\n' "$YELLOW" "$current" "$total" "$RESET" "$BOLD" "$title" "$RESET"
    ui_line
}

ui_ok() {
    printf '%b[OK]%b %s\n' "$GREEN" "$RESET" "$*"
}

ui_note() {
    printf '%b[NOTE]%b %s\n' "$BLUE" "$RESET" "$*"
}

pacman_packages=(
    # Hyprland & Wayland environment
    hyprland hyprlock awww grim slurp wf-recorder swaync waybar
    rofi rofi-emoji yad hyprshot
    xdg-desktop-portal xdg-desktop-portal-hyprland xdg-desktop-portal-wlr xdg-desktop-portal-gtk

    # System services and controls
    brightnessctl network-manager-applet bluez bluez-utils blueman
    pipewire pipewire-pulse wireplumber pavucontrol

    # Apps used by the dotfiles
    ghostty nemo gvfs curl jq python loupe celluloid gnome-text-editor evince
    ffmpeg cava cliphist gnome-characters keepass playerctl wev

    # Qt, display manager, and theming
    sddm qt5ct qt6ct qt5-wayland qt6-wayland nwg-look adw-gtk-theme kvantum-qt5
    sassc gnome-themes-extra

    # Input method
    fcitx5 fcitx5-gtk fcitx5-qt fcitx5-configtool fcitx5-bamboo

    # Fonts and image libraries
    ttf-jetbrains-mono-nerd noto-fonts noto-fonts-emoji
    libvips libheif openslide poppler-glib imagemagick grub
)

aur_packages=(
    # Desktop shell extras
    wlogout sddm-sugar-candy-git apple_cursor whitesur-icon-theme tint

    # Browsers and editors
    brave-bin zen-browser-bin visual-studio-code-bin sublime-text-4

    # Fonts
    ttf-segoe-ui-variable
)

ensure_yay() {
    if command -v yay >/dev/null 2>&1; then
        ui_note "yay already installed."
        return 0
    fi

    local build_dir
    build_dir="$(mktemp -d)"
    git clone https://aur.archlinux.org/yay.git "$build_dir/yay"
    (
        cd "$build_dir/yay"
        makepkg -si --noconfirm
    )
    rm -rf "$build_dir"
}

ui_banner

ui_step 1 4 "Installing base build tools"
sudo pacman -S --needed --noconfirm base-devel git

ui_step 2 4 "Checking AUR helper"
ensure_yay

ui_step 3 4 "Installing official packages"
sudo pacman -S --needed --noconfirm "${pacman_packages[@]}"

ui_step 4 4 "Installing AUR packages"
yay -S --needed --noconfirm "${aur_packages[@]}"

ui_ok "Package install complete."
