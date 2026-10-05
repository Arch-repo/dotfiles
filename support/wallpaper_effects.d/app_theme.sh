#!/usr/bin/env bash

write_zen_theme() {
    local data="${ANTO426_APP_THEME_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/anto-desktop}"
    local resources="$data/zen-browser"
    local palette="$tmp_dir/zen-palette.json"
    local artifact="$tmp_dir/zen-theme"
    ensure_gtk_palette_roles
    if [[ ! -r "$resources/render.py" || ! -r "$data/application-material.json" ]]; then
        log "Zen theme resources missing; run scripts/resources.py"
        return 1
    fi
    application_palette "$palette" || return 1
    python3 "$resources/render.py" --palette "$palette" \
        --material "$data/application-material.json" --output "$artifact" || return 1
    python3 "$(application_theme_helper)" zen "$artifact" "$palette"
}

write_app_theme() {
    # Scrittura dei temi per le app in parallelo
    write_icon_theme &
    pid_icons=$!
    write_gtk_theme &
    pid_gtk=$!
    write_qt_theme &
    pid_qt=$!
    write_zen_theme &
    pid_zen=$!

    wait $pid_icons $pid_gtk $pid_qt $pid_zen

    if declare -F effects_job_is_stale >/dev/null 2>&1 && effects_job_is_stale; then
        log "Skipping stale GTK/Qt reload after app theme write"
        return 0
    fi

    # Ricarica dei temi in parallelo
    gtk_reload_theme &
    pid_gtk_reload=$!
    qt_reload_theme &
    pid_qt_reload=$!

    wait $pid_gtk_reload $pid_qt_reload
}
