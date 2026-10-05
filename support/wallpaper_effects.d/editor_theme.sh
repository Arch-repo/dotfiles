#!/usr/bin/env bash
# Application-specific rendering stays in the theme repos; installation is shared.

application_palette() {
    python3 - "$1" "$background" "$surface" "$base" "$base_alt" "$select" \
        "$accent" "$foreground" "$muted" "$border" "$selected_fg" \
        "$red" "$orange" "$yellow" "$green" "$pink" "$purple" "$gray" \
        "$titlebar" "$titlebar_backdrop" "$popover" <<'PY'
import json,sys
keys='background surface base base_alt select accent foreground muted border selected_fg red orange yellow green pink purple gray titlebar titlebar_backdrop popover'.split()
with open(sys.argv[1],'w') as stream:json.dump(dict(zip(keys,sys.argv[2:],strict=True)),stream)
PY
}

application_theme_helper() {
    local helper="$effects_lib_dir/../compat/app_themes.py"
    [[ -f "$helper" ]] || helper="$script_dir/app_themes.py"
    printf '%s' "$helper"
}

write_vscode_theme() {
    local resources="${ANTO426_APP_THEME_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/anto-desktop}/vscode-theme"
    local palette="$tmp_dir/vscode-palette.json"
    local theme="$tmp_dir/$vscode_theme_file"
    if [[ ! -r "${resources}/cli.cjs" ]]; then
        log "VS Code theme resource missing; run scripts/resources.py"
        return 1
    fi
    application_palette "$palette" || return 1
    node "${resources}/cli.cjs" --palette "$palette" >"$theme" || return 1
    python3 "$(application_theme_helper)" vscode "$theme" "$palette"
}

write_obsidian_theme() {
    local resources="${ANTO426_APP_THEME_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/anto-desktop}/obsidian-theme"
    local palette="$tmp_dir/obsidian-palette.json"
    if [[ ! -r "$resources/template.css" ]]; then
        log "Obsidian theme resource missing; run scripts/resources.py"
        return 1
    fi
    application_palette "$palette" || return 1
    python3 "$(application_theme_helper)" obsidian "$resources" "$palette"
}
