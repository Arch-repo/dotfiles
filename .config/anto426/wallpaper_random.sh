#!/usr/bin/env bash

wallpapers_dir="$HOME/Pictures/Wallpapers"
sync_script="$HOME/.config/anto426/remote_sync.sh"

if [[ -x "$sync_script" ]]; then
    ANTO426_SYNC_QUIET=1 "$sync_script" assets >/dev/null 2>&1 || true
fi

random_wallpaper=$(
    find "$wallpapers_dir" -maxdepth 1 -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \) |
        shuf -n 1
)

[[ -n "$random_wallpaper" ]] || exit 0

awww img "$random_wallpaper" --transition-type any --transition-duration 2

~/.config/anto426/wallpaper_effects.sh "$random_wallpaper"
