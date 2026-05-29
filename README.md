
<p align="center">
  <a href="https://git.io/typing-svg"><img src="https://readme-typing-svg.demolab.com?font=JetBrains+Mono&weight=600&size=24&duration=3000&pause=1000&color=8cb8e4&center=true&vCenter=true&width=500&height=80&lines=Aesthetic+Dotfiles;Wayland+Hyprland+Ecosystem;Dynamic+Color+Theming" alt="Typing SVG" /></a>
</p>

<p align="center">
  <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/divider.gif" width="440" height="40" />
</p>

# <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/icon.gif" width="60px" /> Features;

```sh
root@anto426: ~/dotfiles (main⚡)$ neofetch --view features

- 🎨 Dynamic Colors: Sourced instantly from the wallpaper to GTK, Qt, Rofi, Ghostty, VSCode, and Waybar.
- 🧱 Stable Base: Built on top of the custom Anto426-theme fallback to avoid raw system theme breakages.
- 🖼️ Boot Theme Sync: Generates custom GRUB and SDDM setups matching the wallpaper colors automatically.
- ⚡ Wayland Native: Tuned strictly around modern, high-performance Wayland compositor tools.
- 🧩 Rofi Control Panels: Sliding interactive menus for calendar, audio controls, live brightness, and power.
```

<p align="center">
  <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/divider.gif" width="440" height="40" />
</p>

# <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/icon2.gif" width="70px" /> Applications & Tree;

```sh
root@anto426: ~/dotfiles (main⚡)$ btop --preset applications

+- window.manager                    -+     +- user.experience                   -+
| Hyprland (Compositor/WM)            |     | Waybar (Status Bar)                |
| Ghostty (Terminal Emulator)         |     | SwayNC (Notification Center)        |
| Oh My Posh + Zsh (Prompt Shell)     |     | wlogout (Logout Menus)             |
| Neovim / VS Code (IDE & Editing)    |     | Tmux (Multiplexer & Stow tools)     |
+-------------------------------------+     +-------------------------------------+
```

### 📂 Stow Structure

Managed seamlessly using [GNU Stow](https://www.gnu.org/software/stow/):

```plaintext
.
├── .config/
│   ├── anto426/        # Core wallpaper engine & script configurations
│   ├── cava/           # Audio visualizer layout
│   ├── colors/         # Auto-generated runtime color palettes
│   ├── fontconfig/     # Desktop font renderings
│   ├── ghostty/        # Terminal styling and matching colors
│   ├── hypr/           # Hyprland core rules and keybind configs
│   ├── nvim/           # Custom Neovim IDE modules
│   ├── ohmyposh/       # Terminal prompt skins
│   ├── rofi/           # Custom panels, launchers and dynamic menu themes
│   ├── swaync/         # Notifications center layouts
│   ├── waybar/         # Status bar CSS styling
│   └── wlogout/        # Action menu buttons
├── .tmux.conf          # Window multiplexer rules
└── .zshrc              # Interactive shell configuration
```

<p align="center">
  <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/divider.gif" width="440" height="40" />
</p>

# <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/icon3.gif" width="70px" /> Setup & Install;

```sh
root@anto426: ~/dotfiles (main⚡)$ installer --run
```

### 1. Pacman Bootstrap
Install standard utilities first:
```bash
sudo pacman -S --needed base-devel git stow
```

### 2. Stow Symlinks
Clone this repo to your root `$HOME` and stow the symlinks:
```bash
cd ~
git clone https://github.com/Arch-repo/dotfiles.git
cd dotfiles
stow --restow .
```

### 3. Deploy Packages & Custom Rofi
Build packages and custom Rofi system-wide:
```bash
~/.config/anto426/install_archpkg.sh
```

> [!NOTE]
> The installation script builds the customized Rofi build directly into `/usr`, satisfies standard package managers (`IgnorePkg`), and prevents normal pacman operations from overwriting your custom menus.

### 4. Custom Manual Rofi Compile
If you only need to rebuild the custom slider-enabled Rofi system-wide:
```bash
sudo pacman -S --needed base-devel git meson ninja pkgconf flex bison check pandoc doxygen \
  glib2 cairo pango gdk-pixbuf2 startup-notification libxkbcommon libxcb \
  xcb-util xcb-util-wm xcb-util-cursor xcb-util-keysyms xcb-imdkit \
  wayland wayland-protocols

git clone --recursive https://github.com/Arch-repo/rofi ~/Git/arch/rofi
meson setup ~/Git/arch/rofi/build-anto426 ~/Git/arch/rofi --prefix /usr
meson compile -C ~/Git/arch/rofi/build-anto426
sudo meson install -C ~/Git/arch/rofi/build-anto426
```

Confirm slider compatibility:
```bash
rofi -help | grep slider
```

### 5. Start Theme Engine
Initialize palettes and wallpaper configuration:
```bash
~/.config/anto426/wallpaper_select.sh
```

<p align="center">
  <img src="https://raw.githubusercontent.com/Anto426/Anto426/main/asset/divider.gif" width="440" height="40" />
</p>

<div align="center">
  <i>Configured by anto426</i>
</div>

