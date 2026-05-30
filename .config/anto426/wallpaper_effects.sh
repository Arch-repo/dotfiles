#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
effects_lib_dir="$script_dir/wallpaper_effects.d"
widgets_script="$script_dir/widgets.sh"

destination_wallpaper_dir="$HOME/.cache/awww"
colors_dir="$HOME/.config/colors"
hypr_theme_file="$HOME/.config/hypr/conf/theme.generated.conf"
ghostty_theme_dir="$HOME/.config/ghostty/themes"
htop_config_dir="$HOME/.config/htop"
gtk3_dir="$HOME/.config/gtk-3.0"
gtk4_dir="$HOME/.config/gtk-4.0"
kvantum_dir="$HOME/.config/Kvantum"
kvantum_theme_dir="$kvantum_dir/anto426"
kvantum_config_file="$kvantum_dir/kvantum.kvconfig"
qt5ct_dir="$HOME/.config/qt5ct"
qt6ct_dir="$HOME/.config/qt6ct"
vscode_theme_name="Anto426 Rofi Dynamic"
vscode_theme_file="Anto426-Rofi-Dynamic.json"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_effects.log"
widgets_reload_status="non attivi"

grub_theme_dir="/usr/share/grub/themes/anto426"
grub_background="$grub_theme_dir/background.jpg"
grub_theme="$grub_theme_dir/theme.txt"
grub_select_c="$grub_theme_dir/select_c.png"
grub_select_e="$grub_theme_dir/select_e.png"
grub_select_w="$grub_theme_dir/select_w.png"
sddm_background="/usr/share/sddm/themes/sugar-candy/Backgrounds/current_wallpaper.jpg"
sddm_theme="/usr/share/sddm/themes/sugar-candy/theme.conf"

mkdir -p \
    "$destination_wallpaper_dir" \
    "$colors_dir" \
    "$ghostty_theme_dir" \
    "$htop_config_dir" \
    "$gtk3_dir" \
    "$gtk4_dir" \
    "$kvantum_theme_dir" \
    "$qt5ct_dir/colors" \
    "$qt6ct_dir/colors" \
    "$state_dir" \
    "$(dirname "$hypr_theme_file")"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

notify_theme_reload() {
    notify-send "Theme updated" "$*" 2>/dev/null || true
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || {
        log "Command missing: $1"
        notify "Command missing: $1"
        exit 1
    }
}

for effect_part in "$effects_lib_dir"/*.sh; do
    [[ -r "$effect_part" ]] && source "$effect_part"
done

admin_setup() {
    local script_owner
    local target_user
    local target_group
    local dir
    local file

    if ((EUID != 0)); then
        printf 'Run as root: sudo %s --setup-admin\n' "$0" >&2
        return 1
    fi

    script_owner="$(stat -c '%U' "$0")"
    target_user="${ANTO426_ADMIN_USER:-${SUDO_USER:-$script_owner}}"
    target_group="$(id -gn "$target_user")"

    for dir in "$grub_theme_dir" "$(dirname "$sddm_background")"; do
        mkdir -p "$dir"
        chown "$target_user:$target_group" "$dir"
        chmod u+rwx,go+rx "$dir"
        printf 'OK dir %s -> %s:%s\n' "$dir" "$target_user" "$target_group"
    done

    for file in "$grub_background" "$grub_theme" "$grub_select_c" "$grub_select_e" "$grub_select_w" "$sddm_background" "$sddm_theme"; do
        if [[ -e "$file" ]]; then
            chown "$target_user:$target_group" "$file"
            chmod u+rw,go+r "$file"
            printf 'OK %s -> %s:%s\n' "$file" "$target_user" "$target_group"
        else
            printf 'SKIP missing: %s\n' "$file" >&2
        fi
    done
}

if [[ "${1:-}" == "--setup-admin" ]]; then
    admin_setup
    exit $?
fi

current_wallpaper_path="${1:-$(awww query 2>/dev/null | awk -F'image: ' '/image:/ {print $2; exit}')}"

if [[ -z "$current_wallpaper_path" || ! -f "$current_wallpaper_path" ]]; then
    log "Invalid wallpaper: ${current_wallpaper_path:-empty}"
    exit 0
fi

require_command magick

detect_canvas_size() {
    local size

    if command -v hyprctl >/dev/null 2>&1 && command -v jq >/dev/null 2>&1; then
        size="$(
            hyprctl monitors -j 2>/dev/null |
                jq -r '([.[] | select(.focused == true)][0] // (sort_by(.width * .height) | last)) | select(.width and .height) | "\(.width)x\(.height)"' 2>/dev/null || true
        )"
        [[ "$size" =~ ^[0-9]+x[0-9]+$ ]] && {
            printf '%s' "$size"
            return 0
        }
    fi

    if command -v hyprctl >/dev/null 2>&1; then
        size="$(
            hyprctl monitors 2>/dev/null |
                awk '
                    /^[[:space:]]*[0-9]+x[0-9]+@/ {
                        split($1, mode, "@")
                        split(mode[1], size, "x")
                        area = size[1] * size[2]
                        if (area > best_area) {
                            best_area = area
                            best = mode[1]
                        }
                    }
                    END {
                        if (best != "") print best
                    }
                '
        )"
        [[ "$size" =~ ^[0-9]+x[0-9]+$ ]] && {
            printf '%s' "$size"
            return 0
        }
    fi

    printf '2560x1600'
}

hex_to_rofi_rgba() {
    local hex="${1#\#}"
    local alpha="$2"

    printf 'rgba ( %d, %d, %d, %s %% )' \
        "0x${hex:0:2}" \
        "0x${hex:2:2}" \
        "0x${hex:4:2}" \
        "$alpha"
}

hex_to_css_rgba() {
    local hex="${1#\#}"
    local alpha="$2"

    printf 'rgba(%d, %d, %d, %s)' \
        "0x${hex:0:2}" \
        "0x${hex:2:2}" \
        "0x${hex:4:2}" \
        "$alpha"
}

qt_argb() {
    local hex="${1#\#}"
    local alpha="${2:-ff}"

    printf '#%s%s' "$alpha" "$hex"
}

hypr_rgb() {
    printf 'rgb(%s)' "${1#\#}"
}

hypr_rgba() {
    printf 'rgba(%s%s)' "${1#\#}" "$2"
}

install_file() {
    local src="$1"
    local dst="$2"
    local label="$3"
    local dir
    local setup_cmd

    dir="$(dirname "$dst")"
    setup_cmd="sudo ANTO426_ADMIN_USER=$(id -un) $0 --setup-admin"

    if [[ -e "$dst" && -w "$dst" ]]; then
        cp "$src" "$dst"
        log "$label aggiornato: $dst"
        return 0
    fi

    if [[ -w "$dir" ]]; then
        install -Dm644 "$src" "$dst"
        log "$label aggiornato: $dst"
        return 0
    fi

    if command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
        sudo install -Dm644 "$src" "$dst"
        log "$label aggiornato con sudo: $dst"
        return 0
    fi

    if [[ "${ANTO426_ALLOW_PKEXEC:-0}" == "1" ]] && command -v pkexec >/dev/null 2>&1; then
        if pkexec /usr/bin/install -Dm644 "$src" "$dst"; then
            log "$label aggiornato con pkexec: $dst"
            return 0
        fi
    fi

    log "$label not updated, insufficient permissions: $dst. Setup: $setup_cmd"
    notify "$label not updated: run admin setup once"
    return 1
}

install_optional_file() {
    local src="$1"
    local dst="$2"
    local label="$3"
    local dir

    dir="$(dirname "$dst")"
    if [[ -e "$dst" && -w "$dst" ]] || [[ -w "$dir" ]] || { command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; }; then
        install_file "$src" "$dst" "$label"
        return $?
    fi

    log "$label not updated, insufficient permissions: $dst"
    return 1
}

reload_widgets_after_theme() {
    if [[ -x "$widgets_script" ]]; then
        if "$widgets_script" status 2>/dev/null | grep -q '^running$'; then
            if "$widgets_script" reload >/dev/null 2>&1; then
                widgets_reload_status="reloaded"
            else
                widgets_reload_status="reload error"
                log "Widget reload failed: $widgets_script"
            fi
        else
            log "Widget reload skipped: no active widgets"
        fi
    else
        widgets_reload_status="missing script"
        log "Widget reload skipped, missing script: $widgets_script"
    fi
}

make_cover_image() {
    local src="$1"
    local size="$2"
    local dst="$3"
    local kind="${4:-png}"
    local quality="${5:-92}"
    local args

    args=(
        "$src"
        -auto-orient \
        -resize "${size}^" \
        -gravity center \
        -extent "$size" \
        -strip \
        -colorspace sRGB
    )

    if [[ "$kind" == "jpg" || "$kind" == "jpeg" ]]; then
        args+=(-sampling-factor 4:2:0 -interlace none -quality "$quality" "jpg:$dst")
    else
        args+=("$dst")
    fi

    magick "${args[@]}"
}

make_grub_background() {
    local src="$1"
    local size="$2"
    local dst="$3"
    local width="${size%x*}"
    local height="${size#*x}"
    local panel_x panel_y panel_w panel_h panel_x2 panel_y2
    local footer_x footer_y footer_w footer_h footer_x2 footer_y2

    panel_x=$((width * 14 / 100))
    panel_y=$((height * 16 / 100))
    panel_w=$((width * 72 / 100))
    panel_h=$((height * 58 / 100))
    panel_x2=$((panel_x + panel_w))
    panel_y2=$((panel_y + panel_h))

    footer_x=$((width * 24 / 100))
    footer_y=$((height * 83 / 100))
    footer_w=$((width * 52 / 100))
    footer_h=$((height * 8 / 100))
    footer_x2=$((footer_x + footer_w))
    footer_y2=$((footer_y + footer_h))

    magick "$src" \
        -auto-orient \
        -resize "${size}^" \
        -gravity center \
        -extent "$size" \
        -colorspace sRGB \
        -blur 0x7 \
        -modulate 92,118,100 \
        -fill 'rgba(0,0,0,0.46)' \
        -draw "rectangle 0,0 $width,$height" \
        -fill "$(hex_to_css_rgba "$background" 0.72)" \
        -stroke "$(hex_to_css_rgba "$border" 0.62)" \
        -strokewidth 2 \
        -draw "roundrectangle $panel_x,$panel_y $panel_x2,$panel_y2 34,34" \
        -fill "$(hex_to_css_rgba "$accent" 0.18)" \
        -stroke "$(hex_to_css_rgba "$accent" 0.42)" \
        -strokewidth 1 \
        -draw "roundrectangle $footer_x,$footer_y $footer_x2,$footer_y2 24,24" \
        -fill "$(hex_to_css_rgba "$accent" 0.86)" \
        -stroke none \
        -draw "roundrectangle $((panel_x + panel_w / 2 - width * 4 / 100)),$((panel_y + height * 4 / 100)) $((panel_x + panel_w / 2 + width * 4 / 100)),$((panel_y + height * 4 / 100 + 4)) 3,3" \
        -strip \
        -sampling-factor 4:2:0 \
        -interlace none \
        -quality 92 \
        "jpg:$dst"
}

make_grub_select_pixmaps() {
    local dir="$1"

    magick -size 12x58 xc:"$(hex_to_css_rgba "$accent" 0.46)" \
        -fill "$(hex_to_css_rgba "$foreground" 0.08)" \
        -draw "rectangle 0,1 11,56" \
        PNG32:"$dir/select_c.png"
    magick -size 8x58 xc:"$(hex_to_css_rgba "$accent" 0.46)" PNG32:"$dir/select_e.png"
    magick -size 8x58 xc:"$(hex_to_css_rgba "$accent" 0.46)" PNG32:"$dir/select_w.png"
}

canvas_size="$(detect_canvas_size)"
canvas_width="${canvas_size%x*}"
canvas_height="${canvas_size#*x}"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

log "Updating theme from: $current_wallpaper_path ($canvas_size)"

make_cover_image "$current_wallpaper_path" "$canvas_size" "$destination_wallpaper_dir/normal.png"
printf '%s\n' "$current_wallpaper_path" >"$destination_wallpaper_dir/current-wallpaper.path"

read -r r g b < <(
    magick "$current_wallpaper_path" \
        -auto-orient \
        -resize 1x1\! \
        -format '%[fx:int(255*r)] %[fx:int(255*g)] %[fx:int(255*b)]\n' \
        info:
)
read -r ar ag ab < <(
    magick "$current_wallpaper_path" \
        -auto-orient \
        -resize 1x1\! \
        -modulate 115,170,100 \
        -format '%[fx:int(255*r)] %[fx:int(255*g)] %[fx:int(255*b)]\n' \
        info:
)

generate_palette_from_samples "$r" "$g" "$b" "$ar" "$ag" "$ab"

# Writing themes in parallel for maximum performance
write_session_theme &
pid_session=$!
write_app_theme &
pid_app=$!
write_boot_theme &
pid_boot=$!

wait $pid_session $pid_app $pid_boot


pkill -SIGUSR2 waybar 2>/dev/null || true
swaync-client -rs >/dev/null 2>&1 || true
hyprctl reload >/dev/null 2>&1 || true
reload_widgets_after_theme

log "Theme updated: bg=$background surface=$surface accent=$accent border=$border"
notify_theme_reload "From $(basename "$current_wallpaper_path"). Widgets: $widgets_reload_status. Restart running apps if colors or theme do not update."
