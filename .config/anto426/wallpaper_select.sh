#!/usr/bin/env bash
set -uo pipefail

wallpapers_dir="${ANTO426_WALLPAPERS_DIR:-$HOME/Pictures/Wallpapers}"
theme="$HOME/.config/rofi/config.rasi"
apply_script="$HOME/.config/anto426/wallpaper_apply.sh"
depth="${ANTO426_WALLPAPER_DEPTH:-3}"

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

wallpaper_files() {
    [[ -d "$wallpapers_dir" ]] || return 0
    find "$wallpapers_dir" -maxdepth "$depth" -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \) |
        sort -f
}

current_wallpaper() {
    awww query 2>/dev/null | awk -F'image: ' '/image:/ {print $2; exit}'
}

open_wallpaper_dir() {
    mkdir -p "$wallpapers_dir"
    for opener in nemo dolphin thunar nautilus xdg-open; do
        if command -v "$opener" >/dev/null 2>&1; then
            "$opener" "$wallpapers_dir" >/dev/null 2>&1 &
            return 0
        fi
    done
}

tmp_map="$(mktemp)"
trap 'rm -f "$tmp_map"' EXIT

choice="$(
    {
        printf '󰒟 Wallpaper casuale\n'
        printf '󰑓 Rigenera tema corrente\n'
        printf ' Apri cartella wallpaper\n'
        printf '── Wallpaper ──\n'

        wallpaper_files | while IFS= read -r file; do
            rel="${file#"$wallpapers_dir"/}"
            label="${rel%.*}"
            printf '%s\t%s\n' "$label" "$file" >>"$tmp_map"
            printf '%s\0icon\x1f%s\n' "$label" "$file"
        done
    } |
        rofi -dmenu -i -matching fuzzy \
            -p "Wallpaper" \
            -mesg "Cartella: $wallpapers_dir" \
            -theme "$theme"
)"

case "$choice" in
    "")
        exit 0
        ;;
    *"Wallpaper casuale")
        "$HOME/.config/anto426/wallpaper_random.sh"
        ;;
    *"Rigenera tema corrente")
        current="$(current_wallpaper)"
        if [[ -n "$current" && -f "$current" ]]; then
            "$HOME/.config/anto426/wallpaper_effects.sh" "$current"
            notify "Tema rigenerato"
        else
            notify "Sfondo corrente non trovato"
        fi
        ;;
    *"Apri cartella")
        open_wallpaper_dir
        ;;
    *"Wallpaper"* | "── "*)
        exit 0
        ;;
    *)
        selected_path="$(awk -F'\t' -v label="$choice" '$1 == label {print $2; exit}' "$tmp_map")"
        [[ -n "$selected_path" && -f "$selected_path" ]] || {
            notify "Wallpaper non trovato"
            exit 1
        }
        "$apply_script" "$selected_path"
        ;;
esac
