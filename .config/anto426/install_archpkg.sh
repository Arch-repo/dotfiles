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
    brightnessctl iwd network-manager-applet bluez bluez-utils blueman lm_sensors polkit-gnome
    pipewire pipewire-pulse wireplumber pavucontrol openbsd-netcat

    # Apps used by the dotfiles
    ghostty nemo gvfs curl jq nodejs npm yarn python htop btop loupe celluloid mpv gnome-text-editor evince
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

configure_lid_suspend() {
    local logind_conf="/etc/systemd/logind.conf.d/20-anto426-lid-suspend.conf"
    local tmp

    tmp="$(mktemp)"
    printf '%s\n' \
        '[Login]' \
        'HandleLidSwitch=suspend' \
        'HandleLidSwitchExternalPower=suspend' \
        'HandleLidSwitchDocked=suspend' \
        'LidSwitchIgnoreInhibited=yes' > "$tmp"

    sudo install -Dm644 "$tmp" "$logind_conf"
    rm -f "$tmp"

    if command -v systemctl >/dev/null 2>&1; then
        sudo systemctl kill -s HUP systemd-logind.service >/dev/null 2>&1 || \
            ui_note "Could not reload systemd-logind automatically. Reboot once to activate lid suspend."
    fi

    ui_ok "Lid close configured to suspend, including docked/external-monitor mode."
}

build_anto426_rofi() {
    if [[ "${ANTO426_SKIP_ROFI_BUILD:-0}" == "1" ]]; then
        ui_note "Skipping Anto426 rofi build."
        return 0
    fi

    local rofi_src="${ANTO426_ROFI_SRC:-$HOME/Git/arch/rofi}"
    local rofi_repo="${ANTO426_ROFI_REPO:-https://github.com/Arch-repo/rofi}"
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

build_mpvpaper_from_git() {
    local mpvpaper_src="${ANTO426_MPVPAPER_SRC:-$HOME/Git/arch/mpvpaper}"
    local mpvpaper_repo="${ANTO426_MPVPAPER_REPO:-https://github.com/GhostNaN/mpvpaper.git}"
    local build_dir="${ANTO426_MPVPAPER_BUILD_DIR:-$mpvpaper_src/build-anto426}"
    local prefix="${ANTO426_MPVPAPER_PREFIX:-/usr/local}"

    if [[ ! -d "$mpvpaper_src/.git" ]]; then
        ui_note "Cloning mpvpaper into $mpvpaper_src"
        mkdir -p "$(dirname "$mpvpaper_src")"
        git clone "$mpvpaper_repo" "$mpvpaper_src"
    else
        ui_note "Using existing mpvpaper checkout: $mpvpaper_src"
        (
            cd "$mpvpaper_src"
            git pull --ff-only || ui_note "Could not fast-forward mpvpaper; building the current checkout."
        )
    fi

    if [[ ! -f "$build_dir/build.ninja" ]]; then
        meson setup "$build_dir" "$mpvpaper_src" --prefix "$prefix"
    else
        meson setup --reconfigure "$build_dir" "$mpvpaper_src" --prefix "$prefix"
    fi

    meson compile -C "$build_dir"
    sudo meson install -C "$build_dir"

    if command -v mpvpaper >/dev/null 2>&1; then
        ui_ok "mpvpaper installed from git: $(command -v mpvpaper)"
    else
        ui_note "mpvpaper installed into $prefix/bin; ensure that directory is in PATH."
    fi
}

install_mpvpaper() {
    case "${ANTO426_MPVPAPER_SOURCE:-aur}" in
        git)
            build_mpvpaper_from_git
            return 0
            ;;
        aur | "")
            ;;
        *)
            ui_note "Unknown ANTO426_MPVPAPER_SOURCE=${ANTO426_MPVPAPER_SOURCE}; using AUR."
            ;;
    esac

    if command -v mpvpaper >/dev/null 2>&1; then
        ui_note "mpvpaper already installed: $(command -v mpvpaper)"
        return 0
    fi

    if yay -S --needed --noconfirm mpvpaper; then
        ui_ok "mpvpaper installed from AUR."
        return 0
    fi

    ui_note "AUR install for mpvpaper failed; falling back to git build."
    build_mpvpaper_from_git
}

build_anto426_helper() {
    local source_file="$1"
    local output_file="$2"
    local label="$3"
    local cc_bin="${CC:-cc}"

    if [[ ! -f "$source_file" ]]; then
        ui_note "Skipping $label build; missing source: $source_file"
        return 0
    fi

    if [[ -x "$output_file" && "$output_file" -nt "$source_file" ]]; then
        ui_note "$label already built: $output_file"
        return 0
    fi

    "$cc_bin" -O2 -Wall -Wextra "$source_file" -o "$output_file"
    chmod 755 "$output_file"
    ui_ok "$label built: $output_file"
}

build_anto426_helpers() {
    if [[ "${ANTO426_SKIP_HELPER_BUILD:-0}" == "1" ]]; then
        ui_note "Skipping Anto426 helper builds."
        return 0
    fi

    local script_dir
    local cc_bin="${CC:-cc}"

    script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

    if ! command -v "$cc_bin" >/dev/null 2>&1; then
        ui_note "C compiler not found; install base-devel or set CC before building helpers."
        return 1
    fi

    build_anto426_helper "$script_dir/wallpaper_daemon.c" "$script_dir/wallpaper_daemon" "Wallpaper daemon"
    build_anto426_helper "$script_dir/widgets_core.c" "$script_dir/widgets_core" "Widgets core"
}

ui_banner

ui_step 1 9 "Installing base build tools"
sudo pacman -S --needed --noconfirm base-devel git

ui_step 2 9 "Checking AUR helper"
ensure_yay

ui_step 3 9 "Installing official packages"
sudo pacman -S --needed --noconfirm "${pacman_packages[@]}"

ui_step 4 9 "Installing AUR packages"
yay -S --needed --noconfirm "${aur_packages[@]}"

ui_step 5 9 "Installing wallpaper apps"
install_mpvpaper

ui_step 6 9 "Building local Anto426 apps"
build_anto426_helpers

ui_step 7 9 "Building Anto426 rofi"
build_anto426_rofi

ui_step 8 9 "Configuring NetworkManager iwd backend"
configure_networkmanager_iwd

ui_step 9 9 "Configuring lid suspend"
configure_lid_suspend

ui_ok "Package install complete."
