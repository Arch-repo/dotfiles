#!/usr/bin/env bash
set -uo pipefail

wallpapers_dir="${ANTO426_WALLPAPERS_DIR:-$HOME/Pictures/Wallpapers}"
apply_script="$HOME/.config/anto426/wallpaper_apply.sh"
depth="${ANTO426_WALLPAPER_DEPTH:-3}"

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

random_wallpaper="$(
    find "$wallpapers_dir" -maxdepth "$depth" -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \) 2>/dev/null |
        shuf -n 1
)"

[[ -n "$random_wallpaper" ]] || {
    notify "Nessuno sfondo trovato in $wallpapers_dir"
    exit 0
}

"$apply_script" "$random_wallpaper"
