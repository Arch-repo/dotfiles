#!/usr/bin/env bash

icon_theme_name="${ANTO426_ICON_THEME:-Anto426-Material}"
icon_theme_dir="${XDG_DATA_HOME:-$HOME/.local/share}/icons/$icon_theme_name"
icon_theme_repo="${ANTO426_ICON_THEME_REPO:-$HOME/Git/arch/Anto426-material-icons}"
material_symbol_style="${ANTO426_MATERIAL_SYMBOL_STYLE:-rounded}"
material_symbol_variant="${ANTO426_MATERIAL_SYMBOL_VARIANT:-regular}"

material_symbol_source_svg() {
    local symbol="$1"
    local style suffix candidate
    local styles=("$material_symbol_style" rounded outlined sharp)

    [[ "$material_symbol_variant" == "fill" ]] && suffix="-fill" || suffix=""

    for style in "${styles[@]}"; do
        [[ -n "$style" ]] || continue
        style="${style,,}"
        candidate="$icon_theme_repo/vendor/source/$style/$symbol$suffix.svg"
        if [[ -f "$candidate" ]]; then
            printf '%s\n' "$candidate"
            return 0
        fi
    done

    return 1
}

material_symbol_paths() {
    local source_svg="$1"
    local fill="$2"

    sed -E 's/></>\n</g' "$source_svg" |
        awk -v fill="$fill" '
            /<path[ >]/ {
                gsub(/[[:space:]]fill="[^"]*"/, "")
                sub(/<path/, "<path fill=\"" fill "\"")
                print "    " $0
                found = 1
            }
            END {
                if (!found) exit 1
            }
        '
}

render_material_symbol_svg() {
    local symbol="$1"
    local output="$2"
    local glyph_fill="${3:-${foreground:-#f6f7fb}}"
    local source_svg paths

    if ! source_svg="$(material_symbol_source_svg "$symbol")"; then
        [[ "$symbol" == "apps" ]] || source_svg="$(material_symbol_source_svg apps 2>/dev/null || true)"
    fi

    if [[ -z "${source_svg:-}" ]] || ! paths="$(material_symbol_paths "$source_svg" "$glyph_fill")"; then
        log "Material Symbols SVG non disponibile nella repo: $symbol" 2>/dev/null || true
        return 0
    fi

    mkdir -p "$(dirname "$output")"
    cat >"$output" <<EOF
<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 -960 960 960">
  <g transform="translate(96 -96) scale(0.8)">
$paths
  </g>
</svg>
EOF
}

sync_icon_theme_from_repo() {
    local build_root="$tmp_dir/icon-theme"
    local source_theme="$build_root/$icon_theme_name"

    if [[ ! -x "$icon_theme_repo/scripts/build-theme.sh" ]]; then
        log "Icon theme repo non pronta: $icon_theme_repo" 2>/dev/null || true
        return 1
    fi

    (
        cd "$icon_theme_repo"
        ANTO426_ICON_THEME="$icon_theme_name" \
        ANTO426_ICON_OUTPUT_ROOT="$build_root" \
        ANTO426_MATERIAL_STYLE="$material_symbol_style" \
        ANTO426_MATERIAL_VARIANT="$material_symbol_variant" \
        ANTO426_ICON_FILL="${foreground:-#f6f7fb}" \
        ANTO426_ICON_SURFACE="${surface:-#2d3038}" \
        ANTO426_ICON_ACCENT="${accent:-#8ab4f8}" \
        ANTO426_ICON_SELECT="${select:-#465064}" \
        ANTO426_ICON_BORDER="${border:-#6f7788}" \
            ./scripts/build-theme.sh >/dev/null
    ) || return 1

    [[ -d "$source_theme" ]] || return 1

    mkdir -p "$(dirname "$icon_theme_dir")"
    if command -v rsync >/dev/null 2>&1; then
        mkdir -p "$icon_theme_dir"
        rsync -a --delete "$source_theme/" "$icon_theme_dir/"
    else
        rm -rf "$icon_theme_dir"
        cp -a "$source_theme" "$icon_theme_dir"
    fi
}

write_wlogout_material_icons() {
    local dir="$HOME/.config/wlogout/icons"
    local name symbol

    mkdir -p "$dir"
    while read -r name symbol; do
        [[ -n "$name" && -n "$symbol" ]] || continue
        render_material_symbol_svg "$symbol" "$dir/$name.svg" "${foreground:-#f6f7fb}"
        render_material_symbol_svg "$symbol" "$dir/$name-hover.svg" "${selected_fg:-#11111b}"
    done <<'EOF'
lock lock
suspend bedtime
logout logout
reboot restart_alt
shutdown power_settings_new
cancel close
EOF
}

write_icon_theme() {
    ensure_gtk_palette_roles 2>/dev/null || true

    if ! sync_icon_theme_from_repo; then
        log "Icon theme non aggiornato: controlla $icon_theme_repo" 2>/dev/null || true
        return 0
    fi

    write_wlogout_material_icons

    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -f "$icon_theme_dir" >/dev/null 2>&1 || true
    fi

    log "Icon theme aggiornato da repo: $icon_theme_repo -> $icon_theme_name" 2>/dev/null || true
}
