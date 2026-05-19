#!/usr/bin/env bash

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

wallpapers_dir="$HOME/Pictures/Wallpapers"
theme="$HOME/.config/rofi/config.rasi"
sync_script="$HOME/.config/anto426/remote_sync.sh"

if [[ -x "$sync_script" ]]; then
    ANTO426_SYNC_QUIET=1 "$sync_script" assets >/dev/null 2>&1 || true
fi

selected_wallpaper=$(
    find "$wallpapers_dir" -maxdepth 1 -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \) |
        sort |
        while IFS= read -r a; do
            printf '%s\0icon\x1f%s\n' "$(basename "${a%.*}")" "$a"
        done |
        rofi -dmenu -p " " -theme "$theme"
)

[[ -n "$selected_wallpaper" ]] || exit 0

image_fullname_path=$(find "$wallpapers_dir" -type f -name "$selected_wallpaper.*" | head -n 1)
[[ -n "$image_fullname_path" ]] || exit 0

awww img "$image_fullname_path" --transition-type any --transition-duration 2

~/.config/anto426/wallpaper_effects.sh "$image_fullname_path"
