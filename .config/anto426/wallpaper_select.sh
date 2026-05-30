#!/usr/bin/env bash
set -uo pipefail

wallpapers_dir="${ANTO426_WALLPAPERS_DIR:-$HOME/Pictures/Wallpapers}"
theme="$HOME/.config/rofi/control_menu.rasi"
apply_script="$HOME/.config/anto426/wallpaper_apply.sh"
depth="${ANTO426_WALLPAPER_DEPTH:-3}"

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    exit 0
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

while true; do
    : >"$tmp_map"
    
    # Read wallpaper files to format them properly
    lines=()
    if wallpaper_files | grep -q .; then
        while IFS= read -r file; do
            rel="${file#"$wallpapers_dir"/}"
            label="${rel%.*}"
            lines+=("$label|$file")
        done < <(wallpaper_files)
    fi

    choice="$(
        {
            printf '󰸉 AVAILABLE WALLPAPERS\n'
            count=${#lines[@]}
            if (( count > 0 )); then
                for ((i=0; i<count; i++)); do
                    item="${lines[i]}"
                    lbl="${item%|*}"
                    fl="${item#*|}"
                    printf '%s\t%s\n' "$lbl" "$fl" >>"$tmp_map"
                    if (( i == count - 1 )); then
                        printf ' └─ %s\0icon\x1f%s\n' "$lbl" "$fl"
                    else
                        printf ' ├─ %s\0icon\x1f%s\n' "$lbl" "$fl"
                    fi
                done
            else
                printf ' └─ 󰋼 No wallpapers found\n'
            fi

            printf '\n󰇄 WALLPAPER MANAGEMENT\n'
            printf ' ├─ 󰒟  Random wallpaper\n'
            printf ' ├─ 󰑓  Regenerate current theme\n'
            printf ' └─   Open wallpaper folder\n'
            
            printf '\n󰌍  Back\n'
        } |
            rofi -dmenu -i -matching fuzzy -show-icons \
                -theme-str 'element-icon { width: 96px; height: 54px; border-radius: 6px; }' \
                -p "Wallpaper" \
                -theme "$theme"
    )"

    [[ -z "$choice" ]] && exit 0
    if [[ "$choice" == *"Back"* ]]; then
        go_back
    fi
    if [[ "$choice" != *"├─ "* && "$choice" != *"└─ "* ]]; then
        continue
    fi
    clean_choice="$(printf '%s' "$choice" | sed -E 's/^[[:space:]]*(├─|└─)[[:space:]]*//')"

    case "$clean_choice" in
        *"Nessun wallpaper"* | *"No wallpapers found"*)
            open_wallpaper_dir
            exit 0
            ;;
        *"Wallpaper casuale" | *"Random wallpaper"*)
            "$HOME/.config/anto426/wallpaper_random.sh"
            exit 0
            ;;
        *"Rigenera tema corrente" | *"Regenerate current theme"*)
            current="$(current_wallpaper)"
            if [[ -n "$current" && -f "$current" ]]; then
                "$HOME/.config/anto426/wallpaper_effects.sh" "$current"
                notify "Theme regenerated"
            else
                notify "Current wallpaper not found"
            fi
            exit 0
            ;;
        *"Apri cartella"* | *"Open wallpaper folder"*)
            open_wallpaper_dir
            exit 0
            ;;
        *"Indietro" | *"Back"*)
            go_back
            ;;
        *)
            selected_path="$(awk -F'\t' -v label="$clean_choice" '$1 == label {print $2; exit}' "$tmp_map")"
            [[ -n "$selected_path" && -f "$selected_path" ]] || {
                notify "Wallpaper not found"
                exit 1
            }
            "$apply_script" "$selected_path"
            exit 0
            ;;
    esac
done
