#!/usr/bin/env bash
set -uo pipefail

# Apply a generated palette to the running desktop without rebuilding the
# session. This is the single reload entry point for both wallpaper engines.
#
# The wallpaper path is only an ownership token: a superseded palette job must
# never update a newer desktop. The active wallpaper itself is not modified.
expected_wallpaper="${1:-${ANTO426_WALLPAPER_EFFECTS_EXPECTED:-}}"

config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
local_config_root="${ANTO_LOCAL_CONFIG_ROOT:-$config_home/anto426-local}"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
cache_dir="${XDG_CACHE_HOME:-$HOME/.cache}/awww"
colors_file="$local_config_root/theme/colors.css"
hypr_theme_file="$local_config_root/hypr/theme.generated.conf"
waybar_style_file="$config_home/waybar/style.css"
active_wallpaper_file="$cache_dir/current-wallpaper.path"
lock_file="$state_dir/theme-reload.lock"
signature_file="$state_dir/theme-reload.signature"
log_file="$state_dir/theme-reload.log"
trace_file="${ANTO426_THEME_RELOAD_TRACE:-}"
dry_run="${ANTO426_THEME_RELOAD_DRY_RUN:-0}"

mkdir -p "$state_dir"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

trace() {
    [[ -n "$trace_file" ]] || return 0
    printf '%s\n' "$*" >>"$trace_file"
}

normalize_path() {
    readlink -f -- "$1" 2>/dev/null || printf '%s' "$1"
}

job_is_stale() {
    local active

    [[ -n "$expected_wallpaper" ]] || return 1
    active="$(sed -n '1p' "$active_wallpaper_file" 2>/dev/null || true)"
    [[ -n "$active" ]] || return 1
    [[ "$(normalize_path "$active")" != "$(normalize_path "$expected_wallpaper")" ]]
}

theme_value() {
    local key="$1"

    awk -v key="$key" '
        $1 == key && $2 == "=" {
            print $3
            exit
        }
    ' "$hypr_theme_file" 2>/dev/null
}

valid_hypr_color() {
    [[ "$1" =~ ^rgba?\([[:xdigit:]]{6}([[:xdigit:]]{2})?\)$ ]]
}

palette_signature() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$colors_file" "$hypr_theme_file" 2>/dev/null |
            sha256sum |
            awk '{print $1}'
    else
        cksum "$colors_file" "$hypr_theme_file" 2>/dev/null |
            cksum |
            awk '{print $1 ":" $2}'
    fi
}

run_quiet() {
    trace "$*"
    [[ "$dry_run" == "1" ]] && return 0
    "$@" >/dev/null 2>&1
}

touch_waybar_style() {
    trace "touch $waybar_style_file"
    [[ "$dry_run" == "1" ]] && return 0
    touch -- "$waybar_style_file"
}

reload_waybar() {
    # The generated palette is committed with an atomic rename.  A file
    # monitor attached to the old imported inode may consequently stop
    # following colors.css, so touching style.css is only a fallback.  Waybar
    # officially maps SIGUSR2 to an in-process reload; target the service main
    # process when possible so custom module descendants never receive it.
    if command -v systemctl >/dev/null 2>&1 &&
        systemctl --user --quiet is-active waybar.service 2>/dev/null; then
        if run_quiet systemctl --user kill --signal=SIGUSR2 --kill-whom=main waybar.service; then
            return 0
        fi
        log "Waybar service SIGUSR2 reload failed; trying exact process match"
    fi

    if command -v pgrep >/dev/null 2>&1 &&
        pgrep -x waybar >/dev/null 2>&1 &&
        command -v pkill >/dev/null 2>&1; then
        if run_quiet pkill -SIGUSR2 -x waybar; then
            return 0
        fi
        log "Waybar process SIGUSR2 reload failed; falling back to CSS watcher"
    fi

    # Keep the stylesheet event for an unmanaged build which does not expose a
    # process to this user namespace.  A future Waybar start reads the palette
    # directly even if no process is currently running.
    if [[ -f "$waybar_style_file" ]]; then
        touch_waybar_style
    fi
}

write_signature() {
    local signature="$1"
    local temporary="$signature_file.tmp.$$"

    [[ "$dry_run" == "1" ]] && return 0
    printf '%s\n' "$signature" >"$temporary"
    chmod 600 "$temporary" 2>/dev/null || true
    mv -f -- "$temporary" "$signature_file"
}

# Serialize palette commits. A short quiet period coalesces filesystem events;
# after acquiring the lock we check staleness again.
exec 9>"$lock_file"
if command -v flock >/dev/null 2>&1; then
    flock -x 9
fi
sleep "${ANTO426_THEME_RELOAD_DEBOUNCE_SECONDS:-0.18}"

if job_is_stale; then
    log "Skipped stale palette reload: expected=$expected_wallpaper active=$(sed -n '1p' "$active_wallpaper_file" 2>/dev/null || true)"
    exit 0
fi

if [[ ! -s "$colors_file" || ! -s "$hypr_theme_file" ]]; then
    log "Skipped incomplete palette reload: colors=$colors_file hypr=$hypr_theme_file"
    exit 0
fi

signature="$(palette_signature)"
if [[ -n "$signature" && "$signature" == "$(sed -n '1p' "$signature_file" 2>/dev/null || true)" ]]; then
    log "Skipped unchanged palette reload: $signature"
    exit 0
fi

# Hyprland receives only the three live color keywords that consume generated
# tokens. A full config reload is deliberately forbidden here because it also
# reapplies monitor declarations and window/layer rules.
active_border="$(theme_value '$anto426_active_border')"
inactive_border="$(theme_value '$anto426_inactive_border')"
shadow_color="$(theme_value '$anto426_shadow')"
hypr_batch=""
valid_hypr_color "$active_border" &&
    hypr_batch+="keyword general:col.active_border $active_border ; "
valid_hypr_color "$inactive_border" &&
    hypr_batch+="keyword general:col.inactive_border $inactive_border ; "
valid_hypr_color "$shadow_color" &&
    hypr_batch+="keyword decoration:shadow:color $shadow_color"

if [[ -n "$hypr_batch" ]] && command -v hyprctl >/dev/null 2>&1; then
    run_quiet hyprctl --batch "$hypr_batch" ||
        log "Hyprland targeted color update failed"
fi

# Reload Waybar in place after the imported palette has been atomically
# committed.  The lock/signature above debounce concurrent wallpaper jobs, so
# exactly one SIGUSR2 wave is sent for each distinct palette.
reload_waybar || log "Waybar targeted palette reload failed"

# SwayNC has no imported-CSS watcher, so request one style reload here only.
if command -v swaync-client >/dev/null 2>&1; then
    if command -v timeout >/dev/null 2>&1; then
        run_quiet timeout --foreground 3 swaync-client -rs ||
            log "SwayNC style reload unavailable"
    else
        run_quiet swaync-client -rs ||
            log "SwayNC style reload unavailable"
    fi
fi

# Desktop widgets are Ghostty surfaces. Ghostty officially supports an
# in-place config reload via its user service (SIGUSR2 internally), preserving
# every surface, process, size and position. Fall back to the same supported
# signal only when Ghostty is running outside the service.
if command -v systemctl >/dev/null 2>&1 &&
    systemctl --user --quiet is-active app-com.mitchellh.ghostty.service 2>/dev/null; then
    run_quiet systemctl --user reload app-com.mitchellh.ghostty.service ||
        log "Ghostty service palette reload failed"
elif command -v pgrep >/dev/null 2>&1 &&
    pgrep -x ghostty >/dev/null 2>&1 &&
    command -v pkill >/dev/null 2>&1; then
    run_quiet pkill -SIGUSR2 -x ghostty ||
        log "Ghostty process palette reload failed"
fi

[[ -n "$signature" ]] && write_signature "$signature"
log "Palette committed without session rebuild: ${signature:-no-signature}"
