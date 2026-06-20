#!/usr/bin/env bash
set -uo pipefail

wallpaper="${1:-}"
transition="${ANTO426_WALLPAPER_TRANSITION:-any}"
duration="${ANTO426_WALLPAPER_DURATION:-2}"
effects_script="$HOME/.config/anto426/wallpaper_effects.sh"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_apply.log"
destination_wallpaper_dir="${XDG_CACHE_HOME:-$HOME/.cache}/awww"

mkdir -p "$state_dir" "$destination_wallpaper_dir"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

usage() {
    printf 'Usage: %s /path/wallpaper | --restore\n' "$0" >&2
}

ensure_awww() {
    command -v awww >/dev/null 2>&1 || {
        notify "awww command not found"
        log "awww not found"
        return 1
    }

    if ! pgrep -x awww-daemon >/dev/null 2>&1; then
        awww-daemon >/dev/null 2>&1 &
        sleep 0.25
    fi
}

current_live_wallpaper() {
    local pid
    local candidate

    while read -r pid; do
        [[ -n "$pid" ]] || continue
        candidate="$(
            tr '\0' '\n' <"/proc/$pid/cmdline" 2>/dev/null |
                grep -v '^$' |
                tail -n1
        )"
        if [[ -n "$candidate" && -f "$candidate" ]]; then
            readlink -f "$candidate" 2>/dev/null || printf '%s\n' "$candidate"
            return 0
        fi
    done < <(pgrep -x mpvpaper 2>/dev/null || true)
}

start_wallpaper_daemon() {
    if ! pgrep -f "wallpaper_daemon.sh" >/dev/null 2>&1; then
        "$HOME/.config/anto426/wallpaper_daemon.sh" &
    fi
}

restore_wallpaper() {
    local saved_path
    saved_path="$(cat "$destination_wallpaper_dir/current-wallpaper.path" 2>/dev/null || true)"
    if [[ -n "$saved_path" && -f "$saved_path" ]]; then
        log "Restoring saved wallpaper: $saved_path"
        exec "$0" "$saved_path"
    else
        log "No saved wallpaper to restore"
        exit 0
    fi
}

if [[ "$wallpaper" == "--restore" ]]; then
    restore_wallpaper
fi

case "$wallpaper" in
    "" | "-h" | "--help")
        usage
        exit 2
        ;;
    "~/"*) wallpaper="$HOME/${wallpaper#~/}" ;;
esac

if [[ ! -f "$wallpaper" ]]; then
    notify "Wallpaper not found: $wallpaper"
    log "wallpaper not found: $wallpaper"
    exit 1
fi

wallpaper="$(readlink -f "$wallpaper" 2>/dev/null || printf '%s' "$wallpaper")"

# Check if the file is a video
mime_type="$(file --mime-type -b "$wallpaper" 2>/dev/null || true)"
if [[ "$mime_type" =~ ^video/ ]]; then
    # Ensure mpvpaper is installed
    if ! command -v mpvpaper >/dev/null 2>&1; then
        notify "mpvpaper non è installato per i live wallpaper!"
        log "mpvpaper not found for video wallpaper: $wallpaper"
        exit 1
    fi

    running_live="$(current_live_wallpaper)"
    if [[ -n "$running_live" && "$running_live" == "$wallpaper" ]]; then
        log "live wallpaper already active, skipping mpvpaper restart: $wallpaper"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        start_wallpaper_daemon
        exit 0
    fi

    # Kill any existing mpvpaper
    pkill mpvpaper || true

    log "Applying live wallpaper: $wallpaper"
    ipc_sock="${XDG_RUNTIME_DIR:-/tmp}/mpvpaper-ipc"
    if mpvpaper -p -f -o "no-audio loop --panscan=1.0 --osd-level=0 --input-ipc-server=$ipc_sock" '*' "$wallpaper" >/dev/null 2>&1; then
        log "live wallpaper applied: $wallpaper"
        start_wallpaper_daemon
    else
        notify "Errore nell'avvio di mpvpaper"
        log "mpvpaper failed: $wallpaper"
        exit 1
    fi
else
    # It's a static image
    pkill mpvpaper || true
    ensure_awww || exit 1

    if awww img "$wallpaper" --transition-type "$transition" --transition-duration "$duration"; then
        log "wallpaper applied: $wallpaper"
    else
        notify "Failed to change wallpaper"
        log "awww img failed: $wallpaper"
        exit 1
    fi
fi

if [[ -x "$effects_script" ]]; then
    "$effects_script" "$wallpaper" || log "wallpaper_effects failed: $wallpaper"
else
    log "wallpaper_effects not executable: $effects_script"
fi

notify "Wallpaper applied: $(basename "$wallpaper")"
