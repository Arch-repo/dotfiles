#!/usr/bin/env bash
set -uo pipefail

env_file="${1:-}"
if [[ -z "$env_file" || ! -r "$env_file" ]]; then
    printf 'Usage: %s /path/to/wallpaper_core.env\n' "$0" >&2
    exit 2
fi
shift || true
module_phases=("$@")

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
effects_lib_dir="$script_dir/wallpaper_effects.d"
[[ -d "$effects_lib_dir" ]] || effects_lib_dir="$script_dir/../wallpaper_effects.d"

# shellcheck disable=SC1090
source "$env_file"
# The values are now private to this process; remove only owned snapshots.
[[ "${effects_snapshot:-0}" == "1" ]] && rm -f -- "$env_file"

: "${config_home:=${XDG_CONFIG_HOME:-$HOME/.config}}"
: "${local_config_root:=${ANTO_LOCAL_CONFIG_ROOT:-$config_home/anto426-local}}"
: "${destination_wallpaper_dir:=${XDG_CACHE_HOME:-$HOME/.cache}/awww}"
: "${colors_dir:=$local_config_root/theme}"
: "${hypr_theme_file:=$local_config_root/hypr/theme.generated.conf}"
: "${ghostty_theme_dir:=$HOME/.config/ghostty/themes}"
: "${ghostty_dynamic_file:=$local_config_root/ghostty/dynamic.conf}"
: "${htop_config_dir:=$HOME/.config/htop}"
: "${btop_config_dir:=$HOME/.config/btop}"
: "${btop_theme_dir:=$btop_config_dir/themes}"
: "${gtk3_dir:=$HOME/.config/gtk-3.0}"
: "${gtk4_dir:=$HOME/.config/gtk-4.0}"
: "${kvantum_dir:=$HOME/.config/Kvantum}"
: "${kvantum_theme_dir:=$kvantum_dir/anto426}"
: "${kvantum_config_file:=$kvantum_dir/kvantum.kvconfig}"
: "${qt5ct_dir:=$HOME/.config/qt5ct}"
: "${qt6ct_dir:=$HOME/.config/qt6ct}"
: "${vscode_theme_name:=Anto426 Rofi Dynamic}"
: "${vscode_theme_file:=Anto426-Rofi-Dynamic.json}"
: "${state_dir:=${XDG_STATE_HOME:-$HOME/.local/state}/anto426}"
: "${log_file:=$state_dir/wallpaper_effects.log}"
: "${widgets_script:=$script_dir/widgets.sh}"
: "${grub_theme_dir:=/usr/share/grub/themes/anto426}"
: "${grub_background:=$grub_theme_dir/background.jpg}"
: "${grub_theme:=$grub_theme_dir/theme.txt}"
: "${grub_select_c:=$grub_theme_dir/select_c.png}"
: "${grub_select_e:=$grub_theme_dir/select_e.png}"
: "${grub_select_w:=$grub_theme_dir/select_w.png}"
: "${sddm_theme_dir:=${ANTO426_SDDM_THEME_DIR:-/usr/share/sddm/themes/anto426-sddm}}"
: "${sddm_background:=$sddm_theme_dir/Backgrounds/anto426-current.png}"
: "${sddm_theme:=$sddm_theme_dir/theme.conf}"
: "${canvas_size:=2560x1600}"
: "${canvas_width:=${canvas_size%x*}}"
: "${canvas_height:=${canvas_size#*x}}"
: "${current_wallpaper_path:=${source_wallpaper_path:-}}"

mkdir -p \
    "$destination_wallpaper_dir" \
    "$colors_dir" \
    "$ghostty_theme_dir" \
    "$htop_config_dir" \
    "$btop_theme_dir" \
    "$gtk3_dir" \
    "$gtk4_dir" \
    "$kvantum_theme_dir" \
    "$qt5ct_dir/colors" \
    "$qt6ct_dir/colors" \
    "$state_dir" \
    "$(dirname "$hypr_theme_file")" \
    "$(dirname "$ghostty_dynamic_file")"

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

normalize_path() {
    readlink -f "$1" 2>/dev/null || printf '%s' "$1"
}

module_job_is_stale() {
    local expected="${effects_expected:-${source_wallpaper_path:-}}"
    local active

    # Boot/login is a separate persistent domain.  It deliberately never
    # shares the desktop current-wallpaper ownership token.
    [[ "${effects_domain:-desktop}" == "desktop" ]] || return 1
    [[ -n "$expected" ]] || return 1
    active="$(cat "$destination_wallpaper_dir/current-wallpaper.path" 2>/dev/null || true)"
    [[ -n "$active" ]] || return 1
    [[ "$(normalize_path "$active")" != "$(normalize_path "$expected")" ]]
}

exit_if_stale_module_job() {
    if module_job_is_stale; then
        log "Wallpaper module bridge skipped stale job: expected=${effects_expected:-${source_wallpaper_path:-}} active=$(cat "$destination_wallpaper_dir/current-wallpaper.path" 2>/dev/null || true)"
        exit 0
    fi
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

install_file() {
    local src="$1"
    local dst="$2"
    local label="$3"
    local dir
    local setup_cmd

    dir="$(dirname "$dst")"
    setup_cmd="sudo ANTO426_ADMIN_USER=$(id -un) $script_dir/wallpaper_effects.sh --setup-admin"

    # The boot/login selector renders a private generation first.  This branch
    # is deliberately user-writable and can never fall through to sudo/pkexec.
    if [[ -n "${boot_theme_stage_dir:-}" &&
          "$dst" == "$boot_theme_stage_dir"/* ]]; then
        install -Dm644 "$src" "$dst"
        log "$label staged: $dst"
        return 0
    fi

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
    if [[ -n "${boot_theme_stage_dir:-}" &&
          "$dst" == "$boot_theme_stage_dir"/* ]]; then
        install -Dm644 "$src" "$dst"
        log "$label staged: $dst"
        return 0
    fi
    if [[ -e "$dst" && -w "$dst" ]] || [[ -w "$dir" ]] || { command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; }; then
        install_file "$src" "$dst" "$label"
        return $?
    fi

    log "$label not updated, insufficient permissions: $dst"
    return 1
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
        -auto-orient
        -resize "${size}^"
        -gravity center
        -extent "$size"
        -strip
        -colorspace sRGB
    )

    if [[ "$kind" == "jpg" || "$kind" == "jpeg" ]]; then
        args+=(-sampling-factor 4:2:0 -interlace none -quality "$quality" "jpg:$dst")
    else
        args+=("$dst")
    fi

    magick "${args[@]}"
}

for effect_part in \
    "$effects_lib_dir/session_theme.sh" \
    "$effects_lib_dir/app_theme.sh" \
    "$effects_lib_dir/icon_theme.sh" \
    "$effects_lib_dir/boot_theme.sh"; do
    [[ -r "$effect_part" ]] && source "$effect_part"
done

exit_if_stale_module_job

phase_requested() {
    local phase="$1"
    local requested

    if ((${#module_phases[@]} == 0)); then
        return 0
    fi

    for requested in "${module_phases[@]}"; do
        case "$requested" in
            all)
                return 0 ;;
            apps)
                case "$phase" in
                    gtk | qt | kvantum | zen | icons) return 0 ;;
                esac
                ;;
            session)
                [[ "$phase" == "vscode" ]] && return 0
                ;;
            "$phase")
                return 0
                ;;
        esac
    done

    return 1
}

icon_theme_signature() {
    {
        printf 'theme=%s\n' "${icon_theme_name:-${ANTO426_ICON_THEME:-Anto426-Material}}"
        printf 'repo=%s\n' "${icon_theme_repo:-${ANTO426_ICON_THEME_REPO:-$HOME/Git/arch/Anto426-material-icons}}"
        printf 'style=%s\n' "${material_symbol_style:-${ANTO426_MATERIAL_SYMBOL_STYLE:-rounded}}"
        printf 'variant=%s\n' "${material_symbol_variant:-${ANTO426_MATERIAL_SYMBOL_VARIANT:-regular}}"
        printf 'foreground=%s\n' "${foreground:-}"
        printf 'surface=%s\n' "${surface:-}"
        printf 'accent=%s\n' "${accent:-}"
        printf 'select=%s\n' "${select:-}"
        printf 'border=%s\n' "${border:-}"
        printf 'selected_fg=%s\n' "${selected_fg:-}"
    } | sha1sum | awk '{print $1}'
}

write_icon_theme_cached() {
    local sig_file="$state_dir/icon-theme.signature"
    local signature
    local old_signature

    if [[ "${ANTO426_ICON_THEME_FORCE:-0}" != "1" ]]; then
        signature="$(icon_theme_signature)"
        old_signature="$(cat "$sig_file" 2>/dev/null || true)"
        if [[ -n "$signature" && "$signature" == "$old_signature" ]]; then
            log "Icon theme saltato: palette e stile invariati"
            return 0
        fi
    else
        signature="$(icon_theme_signature)"
    fi

    ensure_gtk_palette_roles 2>/dev/null || true
    if ! sync_icon_theme_from_repo; then
        log "Icon theme non aggiornato: controlla $icon_theme_repo"
        return 0
    fi

    write_wlogout_material_icons

    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -f "$icon_theme_dir" >/dev/null 2>&1 || true
    fi

    mkdir -p "$(dirname "$sig_file")"
    printf '%s\n' "$signature" >"$sig_file"
    log "Icon theme aggiornato da repo: $icon_theme_repo -> $icon_theme_name"
}

visible_pids=()
visible_labels=()
deferred_pids=()
deferred_labels=()

launch_visible() {
    local label="$1"
    shift
    "$@" &
    visible_pids+=("$!")
    visible_labels+=("$label")
}

launch_deferred() {
    local label="$1"
    shift
    "$@" &
    deferred_pids+=("$!")
    deferred_labels+=("$label")
}

wait_group() {
    local status=0
    local pid

    shift
    for pid in "$@"; do
        wait "$pid" || status=1
    done

    return "$status"
}

# Boot staging can be called by a desktop job that already owns its lock.
# Each domain serializes its writers without recursively taking the same lock.
exec 8>"$state_dir/${effects_domain:-desktop}-theme-modules.lock"
flock -x 8
exit_if_stale_module_job

visible_status=0
deferred_status=0

if [[ "${ANTO426_WALLPAPER_CORE_APPS:-1}" != "0" ]]; then
    phase_requested gtk && launch_visible gtk write_gtk_theme
    phase_requested qt && launch_visible qt write_qt_theme
    phase_requested kvantum && launch_visible kvantum write_kvantum_theme
    phase_requested zen && launch_visible zen write_zen_theme
fi

if ((${#visible_pids[@]} > 0)); then
    wait_group visible "${visible_pids[@]}" || visible_status=1
    exit_if_stale_module_job

    gtk_reload_theme &
    pid_gtk_reload=$!
    qt_reload_theme &
    pid_qt_reload=$!
    wait "$pid_gtk_reload" "$pid_qt_reload" || visible_status=1
fi

if [[ "${ANTO426_WALLPAPER_CORE_VSCODE:-1}" != "0" ]]; then
    phase_requested vscode && launch_deferred vscode write_vscode_theme
fi

if [[ "${ANTO426_WALLPAPER_CORE_APPS:-1}" != "0" ]]; then
    phase_requested icons && launch_deferred icons write_icon_theme_cached
fi

if [[ "${ANTO426_WALLPAPER_CORE_BOOT:-1}" != "0" ]]; then
    phase_requested boot && launch_deferred boot write_boot_theme
fi

if ((${#deferred_pids[@]} > 0)); then
    exit_if_stale_module_job
    wait_group deferred "${deferred_pids[@]}" || deferred_status=1
fi

exit $((visible_status || deferred_status))
