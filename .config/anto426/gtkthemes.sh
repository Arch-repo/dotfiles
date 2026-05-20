#!/usr/bin/env bash

SCHEME="prefer-dark"
THEME="${ANTO426_GTK_THEME:-anto426}"
ICONS="WhiteSur-dark"
CURSOR="macOS"
UI_FONT="Segoe UI Variable Static Text 12"
MONO_FONT="JetBrainsMono Nerd Font 12"

SCHEMA="gsettings set org.gnome.desktop.interface"

theme_exists() {
    local theme="$1"

    [[ -d "$HOME/.themes/$theme" ]] ||
        [[ -d "$HOME/.local/share/themes/$theme" ]] ||
        [[ -d "/usr/share/themes/$theme" ]]
}

resolve_theme() {
    local candidate

    for candidate in "$THEME" anto426 Anto426-Dark Anto426 adw-gtk3-dark Adwaita-dark Adwaita; do
        if theme_exists "$candidate"; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    printf '%s\n' "$THEME"
}

write_gtk_settings() {
    local selected_theme="$1"
    local gtk_settings_dir

    for gtk_settings_dir in "$HOME/.config/gtk-3.0" "$HOME/.config/gtk-4.0"; do
        mkdir -p "$gtk_settings_dir"
        cat >"$gtk_settings_dir/settings.ini" <<EOF
[Settings]
gtk-theme-name=$selected_theme
gtk-icon-theme-name=$ICONS
gtk-cursor-theme-name=$CURSOR
gtk-font-name=$UI_FONT
gtk-application-prefer-dark-theme=true
EOF
    done
}

apply_themes() {
    local selected_theme

    selected_theme="$(resolve_theme)"
    write_gtk_settings "$selected_theme"
    ${SCHEMA} color-scheme "$SCHEME"
    ${SCHEMA} gtk-theme "$selected_theme"
    ${SCHEMA} icon-theme "$ICONS"
    ${SCHEMA} cursor-theme "$CURSOR"
    ${SCHEMA} font-name "$UI_FONT"
    ${SCHEMA} monospace-font-name "$MONO_FONT"
}

apply_themes
