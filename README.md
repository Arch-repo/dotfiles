<div align="center">

# 🌌 anto426 dotfiles

[![Typing SVG](https://readme-typing-svg.demolab.com?font=JetBrains+Mono&weight=600&size=28&duration=3000&pause=1000&color=8cb8e4&center=true&vCenter=true&width=435&lines=Welcome+to+my+Dotfiles;Aesthetic+Wayland+Setup;Dynamic+Color+Engine)](https://git.io/typing-svg)

A highly customized, aesthetic, and fully dynamic **Wayland/Hyprland** ecosystem. 

</div>

---

## ✨ Key Features

This setup is not just a collection of config files; it features a **fully dynamic theming engine** that extracts colors from the current wallpaper and propagates them instantly across the entire operating system.

- 🎨 **Dynamic Colors**: Instant palette generation for GTK, Qt, Kvantum, Rofi, Ghostty, and Waybar.
- 🧱 **Theme Fallbacks**: Uses the separate [`Anto426-theme`](https://github.com/Anto426/Anto426-theme) repo as a stable GTK base before dynamic colors are generated.
- 🖼️ **Boot Integration**: Custom SDDM and GRUB themes automatically generated to match the wallpaper.
- ⚡ **Performance**: Built around modern, fast tools optimized for Wayland.
- 🧩 **Modular Control Menu**: Integrated Rofi-based menus for audio, bluetooth, Wi-Fi, a synchronized calendar, live sliders, and background app switching.

---

## 📦 Stack & Applications

| Category | Tool |
|----------|------|
| **Window Manager** | [Hyprland](https://hyprland.org/) |
| **Status Bar** | [Waybar](https://github.com/Alexays/Waybar) |
| **App Launcher** | [Anto426 Rofi](https://github.com/Anto426/rofi), built with Wayland and slider support |
| **Terminal** | [Ghostty](https://github.com/mitchellh/ghostty) |
| **Notifications** | [SwayNC](https://github.com/ErikReider/SwayNotificationCenter) |
| **Editor** | [Neovim](https://neovim.io/) |
| **Shell & Prompt**| [Zsh](https://www.zsh.org/) + [Oh My Posh](https://ohmyposh.dev/) |
| **Multiplexer** | [Tmux](https://github.com/tmux/tmux) |
| **Logout Menu** | [wlogout](https://github.com/ArtsyMacaw/wlogout) |

---

## 📂 Repository Structure

The repository is structured to be managed seamlessly with [GNU Stow](https://www.gnu.org/software/stow/).

```plaintext
.
├── .config/
│   ├── anto426/        # Core theme engine & shell scripts
│   ├── cava/           # Audio visualizer config
│   ├── colors/         # Auto-generated color schemes
│   ├── fontconfig/     # Font rendering configurations
│   ├── ghostty/        # Terminal configs & dynamic palettes
│   ├── hypr/           # Hyprland window manager rules & settings
│   ├── nvim/           # Neovim IDE configuration
│   ├── ohmyposh/       # Terminal prompt styling
│   ├── rofi/           # Menus, calendar and app launcher themes
│   ├── swaync/         # Notification center styling
│   ├── waybar/         # Status bar layout and CSS
│   └── wlogout/        # Power menu
├── .stow-local-ignore  # Files ignored by Stow
├── .tmux.conf          # Tmux configuration
├── .zshrc              # Zsh shell configuration
└── LICENSE             # MIT License
```

---

## 🚀 Installation

### 1. Base Packages
On Arch, install the bootstrap tools first:

```bash
sudo pacman -S --needed base-devel git stow
```

### 2. Clone & Stow
Clone this repository into your home folder and use `stow` to create the symlinks:

```bash
cd ~
git clone https://github.com/Anto426/dotfiles.git
cd dotfiles
stow --restow .
```

### 3. Install Arch/Hyprland Packages
The installer pulls the package set used by this Arch-Hyprland setup, installs AUR extras through `yay`, then builds the custom rofi used by the control menus:

```bash
~/.config/anto426/install_archpkg.sh
```

The script builds [`Anto426/rofi`](https://github.com/Anto426/rofi) into:

```bash
~/.local/rofi-anto426
```

and links the active binary here:

```bash
~/.local/bin/rofi
```

Make sure `~/.local/bin` is before `/usr/bin` in your session `PATH`.

### 4. Manual Rofi Build
If you only want to rebuild the custom rofi:

```bash
sudo pacman -S --needed base-devel git meson ninja pkgconf flex bison check pandoc doxygen \
  glib2 cairo pango gdk-pixbuf2 startup-notification libxkbcommon libxcb \
  xcb-util xcb-util-wm xcb-util-cursor xcb-util-keysyms xcb-imdkit \
  wayland wayland-protocols

git clone --recursive https://github.com/Anto426/rofi ~/Git/arch/rofi
meson setup ~/Git/arch/rofi/build-anto426 ~/Git/arch/rofi --prefix ~/.local/rofi-anto426
meson compile -C ~/Git/arch/rofi/build-anto426
meson install -C ~/Git/arch/rofi/build-anto426
mkdir -p ~/.local/bin
ln -sfn ~/.local/rofi-anto426/bin/rofi ~/.local/bin/rofi
```

Verify the slider-enabled build:

```bash
~/.local/bin/rofi -help | grep slider
```

### 5. Initialize the Theme Engine
To generate the initial color palettes and apply the theme to the entire system (including GRUB and Qt), simply run the wallpaper engine once or select a wallpaper from the Rofi menu:

```bash
~/.config/anto426/wallpaper_select.sh
```

---

<div align="center">
  <i>Configured by anto426</i>
</div>

## Theme Base

The GTK fallback/base theme lives in [`Anto426-theme`](https://github.com/Anto426/Anto426-theme), a renamed and tuned fork of [vinceliuice/Orchis-theme](https://github.com/vinceliuice/Orchis-theme).
