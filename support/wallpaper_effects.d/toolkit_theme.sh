#!/usr/bin/env bash
# Toolkit renderers and base assets belong to their checksum-pinned theme repos.

ensure_gtk_palette_roles() {
    : "${base:=$background}" "${base_alt:=$surface}" "${titlebar:=$surface}"
    : "${popover:=$surface}" "${titlebar_backdrop:=$popover}" "${selected_fg:=$foreground}"
}

write_toolkit_theme() {
    local toolkit="$1"
    local data="${ANTO426_APP_THEME_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/anto-desktop}"
    local resources="$data/$toolkit-theme"
    local palette="$tmp_dir/$toolkit-palette.json"
    local artifact="$tmp_dir/$toolkit-theme"
    ensure_gtk_palette_roles
    if [[ ! -r "$resources/render.py" || ! -r "$data/application-material.json" ]]; then
        log "$toolkit theme resources missing; install the desktop and run scripts/resources.py"
        return 1
    fi
    application_palette "$palette" || return 1
    python3 "$resources/render.py" --palette "$palette" \
        --material "$data/application-material.json" --output "$artifact" || return 1
    python3 "$(application_theme_helper)" "$toolkit" "$artifact" "$palette"
}

write_gtk_theme() { write_toolkit_theme gtk; }
write_qt_theme() { write_toolkit_theme qt; }
write_kvantum_theme() { write_qt_theme; }

gsettings_set_if_changed() {
    local schema="$1" key="$2" wanted="$3" current
    current="$(gsettings get "$schema" "$key" 2>/dev/null || true)"
    current="${current#\'}"; current="${current%\'}"
    [[ "$current" == "$wanted" ]] || gsettings set "$schema" "$key" "$wanted" 2>/dev/null || true
}

gtk_reload_theme() {
    if command -v gsettings >/dev/null 2>&1; then
        gsettings_set_if_changed org.gnome.desktop.interface gtk-theme anto426
        gsettings_set_if_changed org.gnome.desktop.interface color-scheme prefer-dark
    fi
    log "GTK palette installed; applications manage their own stylesheet reload"
}

qt_reload_theme() {
    # Qt6ct exposes qt5ct as a compatible key; the same key exists in Qt5ct.
    # Native palette files are separate, so Qt6 receives its additional Accent role.
    log "Qt5/Qt6 and Kvantum palette installed; applications manage their own style reload"
}
