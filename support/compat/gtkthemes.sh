#!/usr/bin/env bash

SCHEME="prefer-dark"
THEME="${ANTO426_GTK_THEME:-anto426}"
ICONS="${ANTO426_ICON_THEME:-Anto426-Material}"
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
    python3 - "${BASH_SOURCE[0]}" "$selected_theme" "$ICONS" "$CURSOR" "$UI_FONT" <<'PY'
import importlib.util,os,sys
from pathlib import Path
spec=importlib.util.spec_from_file_location('app_themes',Path(sys.argv[1]).resolve().parent/'app_themes.py')
helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
root=Path(os.environ.get('XDG_CONFIG_HOME',Path.home()/'.config'))
for version in ('3.0','4.0'):
    entries=dict(zip(('gtk-theme-name','gtk-icon-theme-name','gtk-cursor-theme-name','gtk-font-name'),sys.argv[2:],strict=True))
    entries['gtk-application-prefer-dark-theme']='true' if version=='3.0' else None
    helper.update_ini(root/('gtk-'+version)/'settings.ini',{'Settings':entries})
PY
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
