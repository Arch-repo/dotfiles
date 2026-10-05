#!/usr/bin/env bash

write_sddm_theme() {
    local theme_source="${sddm_theme_template:-$sddm_theme}"

    make_cover_image "$current_wallpaper_path" "$canvas_size" "$tmp_dir/sddm-background.png" png 94
    install_file "$tmp_dir/sddm-background.png" "$sddm_background" "SDDM background" || true

    if [[ -f "$theme_source" ]]; then
        awk \
        -v bg="$background" \
        -v surface="$surface" \
        -v fg="$foreground" \
        -v muted="$muted" \
        -v accent="$accent" \
        -v border="$border" \
        -v selected_fg="$selected_fg" \
        -v red="$red" \
        -v width="$canvas_width" \
        -v height="$canvas_height" '
        /^Background=/ { print "Background=\"Backgrounds/anto426-current.png\""; next }
        /^ScreenWidth=/ { print "ScreenWidth=\"" width "\""; next }
        /^ScreenHeight=/ { print "ScreenHeight=\"" height "\""; next }
        /^MainColor=/ { print "MainColor=\"" fg "\""; next }
        /^AccentColor=/ { print "AccentColor=\"" accent "\""; next }
        /^BackgroundColor=/ { print "BackgroundColor=\"" bg "\""; next }
        /^OverrideLoginButtonTextColor=/ { print "OverrideLoginButtonTextColor=\"" selected_fg "\""; next }
        /^SurfaceColor=/ { print "SurfaceColor=\"" surface "\""; next }
        /^BorderColor=/ { print "BorderColor=\"" border "\""; next }
        /^MutedColor=/ { print "MutedColor=\"" muted "\""; next }
        /^ErrorColor=/ { print "ErrorColor=\"" red "\""; next }
            { print }
        ' "$theme_source" >"$tmp_dir/sddm-theme.conf"
    else
        cat >"$tmp_dir/sddm-theme.conf" <<EOF
[General]
Background="Backgrounds/anto426-current.png"
ScreenWidth="$canvas_width"
ScreenHeight="$canvas_height"
MainColor="$foreground"
AccentColor="$accent"
BackgroundColor="$background"
OverrideLoginButtonTextColor="$selected_fg"
SurfaceColor="$surface"
BorderColor="$border"
MutedColor="$muted"
ErrorColor="$red"
EOF
    fi
    install_file "$tmp_dir/sddm-theme.conf" "$sddm_theme" "SDDM theme" || true
}

write_grub_theme() {
    "$script_dir/boot_render.py" "$tmp_dir" "$current_wallpaper_path" "$canvas_size" "$background" "$foreground" "$accent" || return 1
    install_file "$tmp_dir/grub-background.jpg" "$grub_background" "GRUB background" || return 1
    install_file "$tmp_dir/grub-theme.txt" "$grub_theme" "GRUB theme" || return 1
    local part
    for part in c n ne e se s sw w nw; do
        install_file "$tmp_dir/select_$part.png" "$grub_theme_dir/select_$part.png" "GRUB selection $part" || return 1
    done
}

write_boot_theme() {
    if [[ "${effects_domain:-desktop}" == "desktop" ]]; then
        local apply_script="${ANTO426_WALLPAPER_BOOT_APPLY_SCRIPT:-$script_dir/wallpaper_boot_apply.sh}"
        exit_if_stale_module_job
        if ! "$apply_script" "${source_wallpaper_path:-$current_wallpaper_path}"; then
            log "Boot/login theme update failed"
            notify "Tema GRUB e login non aggiornato; controlla il log degli sfondi"
            return 1
        fi
        return 0
    fi
    write_grub_theme
    write_sddm_theme
}
