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
    brightnessctl iwd network-manager-applet bluez bluez-utils blueman
    pipewire pipewire-pulse wireplumber pavucontrol

    # Apps used by the dotfiles
    ghostty nemo gvfs curl jq nodejs npm yarn python htop loupe celluloid gnome-text-editor evince
    ffmpeg cava cliphist gnome-characters keepass playerctl wev

    # Qt, display manager, and theming
    sddm qt5ct qt6ct qt5-wayland qt6-wayland nwg-look adw-gtk-theme kvantum-qt5
    sassc gnome-themes-extra

    # Input method
    fcitx5 fcitx5-gtk fcitx5-qt fcitx5-configtool fcitx5-bamboo

    # Fonts and image libraries
    ttf-jetbrains-mono-nerd noto-fonts noto-fonts-emoji
    libvips libheif openslide poppler-glib imagemagick grub

    # Build dependencies for Anto426 rofi with slider support
    stow meson ninja pkgconf flex bison check pandoc doxygen
    glib2 cairo pango gdk-pixbuf2 startup-notification
    libxkbcommon libxcb xcb-util xcb-util-wm xcb-util-cursor xcb-util-keysyms xcb-imdkit
    wayland wayland-protocols
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

configure_networkmanager_iwd() {
    local nm_conf="/etc/NetworkManager/conf.d/wifi_backend.conf"
    local iwd_conf="/etc/iwd/main.conf"
    local tmp

    tmp="$(mktemp)"
    printf '%s\n' \
        '[device]' \
        'wifi.backend=iwd' \
        'wifi.iwd.autoconnect=false' > "$tmp"
    sudo install -Dm644 "$tmp" "$nm_conf"
    rm -f "$tmp"

    if [[ ! -f "$iwd_conf" ]]; then
        tmp="$(mktemp)"
        printf '%s\n' \
            '[General]' \
            'EnableNetworkConfiguration=false' > "$tmp"
        sudo install -Dm644 "$tmp" "$iwd_conf"
        rm -f "$tmp"
    fi

    if command -v systemctl >/dev/null 2>&1; then
        sudo systemctl enable --now iwd.service >/dev/null 2>&1 || true
        sudo systemctl disable --now wpa_supplicant.service >/dev/null 2>&1 || true

        if [[ "${ANTO426_SKIP_NETWORK_RESTART:-0}" == "1" ]]; then
            ui_note "NetworkManager restart skipped. Restart it later to activate the iwd backend."
        else
            sudo systemctl restart NetworkManager.service >/dev/null 2>&1 || \
                ui_note "Could not restart NetworkManager automatically. Restart it to activate the iwd backend."
        fi
    fi

    ui_ok "NetworkManager configured to use iwd for Wi-Fi."
}

build_anto426_rofi() {
    if [[ "${ANTO426_SKIP_ROFI_BUILD:-0}" == "1" ]]; then
        ui_note "Skipping Anto426 rofi build."
        return 0
    fi

    local rofi_src="${ANTO426_ROFI_SRC:-$HOME/Git/arch/rofi}"
    local rofi_repo="${ANTO426_ROFI_REPO:-https://github.com/Anto426/rofi}"
    local build_dir="${ANTO426_ROFI_BUILD_DIR:-$rofi_src/build-anto426}"
    local prefix="${ANTO426_ROFI_PREFIX:-/usr}"

    if [[ ! -d "$rofi_src/.git" ]]; then
        ui_note "Cloning Anto426 rofi into $rofi_src"
        mkdir -p "$(dirname "$rofi_src")"
        git clone --recursive "$rofi_repo" "$rofi_src"
    else
        ui_note "Using existing Anto426 rofi checkout: $rofi_src"
        (
            cd "$rofi_src"
            git submodule update --init --recursive
        )
    fi

    if [[ ! -f "$build_dir/build.ninja" ]]; then
        meson setup "$build_dir" "$rofi_src" --prefix "$prefix"
    else
        meson setup --reconfigure "$build_dir" "$rofi_src" --prefix "$prefix"
    fi

    meson compile -C "$build_dir"
    sudo meson install -C "$build_dir"

    # Clean up old local build if it exists
    rm -rf "$HOME/.local/rofi-anto426"
    rm -f "$HOME/.local/bin/rofi"

    # Prevent future system updates from overwriting the custom build
    if ! grep -q "^IgnorePkg.*=.*rofi" /etc/pacman.conf; then
        ui_note "Adding rofi to IgnorePkg in /etc/pacman.conf..."
        if grep -q "^#IgnorePkg" /etc/pacman.conf; then
            sudo sed -i 's/^#IgnorePkg\s*=/IgnorePkg = rofi/' /etc/pacman.conf
        else
            sudo sed -i '/\[options\]/a IgnorePkg = rofi' /etc/pacman.conf
        fi
    fi

    if "/usr/bin/rofi" -help 2>&1 | grep -Fq -- "-slider-change-command"; then
        ui_ok "Anto426 rofi installed system-wide with slider support: /usr/bin/rofi"
    else
        ui_note "Anto426 rofi installed, but slider dmenu option was not detected in help output."
    fi
}

ui_banner

ui_step 1 6 "Installing base build tools"
sudo pacman -S --needed --noconfirm base-devel git

ui_step 2 6 "Checking AUR helper"
ensure_yay

ui_step 3 6 "Installing official packages"
sudo pacman -S --needed --noconfirm "${pacman_packages[@]}"

ui_step 4 6 "Installing AUR packages"
yay -S --needed --noconfirm "${aur_packages[@]}"

ui_step 5 6 "Building Anto426 rofi"
build_anto426_rofi

ui_step 6 6 "Configuring NetworkManager iwd backend"
configure_networkmanager_iwd

ui_ok "Package install complete."
