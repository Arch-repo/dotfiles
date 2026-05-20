#!/usr/bin/env bash
set -uo pipefail

wallpaper="${1:-}"
transition="${ANTO426_WALLPAPER_TRANSITION:-any}"
duration="${ANTO426_WALLPAPER_DURATION:-2}"
effects_script="$HOME/.config/anto426/wallpaper_effects.sh"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_apply.log"

mkdir -p "$state_dir"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

usage() {
    printf 'Uso: %s /path/wallpaper\n' "$0" >&2
}

ensure_awww() {
    command -v awww >/dev/null 2>&1 || {
        notify "Comando awww non trovato"
        log "awww non trovato"
        return 1
    }

    if ! pgrep -x awww-daemon >/dev/null 2>&1; then
        awww-daemon >/dev/null 2>&1 &
        sleep 0.25
    fi
}

case "$wallpaper" in
    "" | "-h" | "--help")
        usage
        exit 2
        ;;
    "~/"*) wallpaper="$HOME/${wallpaper#~/}" ;;
esac

if [[ ! -f "$wallpaper" ]]; then
    notify "Sfondo non trovato: $wallpaper"
    log "sfondo non trovato: $wallpaper"
    exit 1
fi

wallpaper="$(readlink -f "$wallpaper" 2>/dev/null || printf '%s' "$wallpaper")"

ensure_awww || exit 1

if awww img "$wallpaper" --transition-type "$transition" --transition-duration "$duration"; then
    log "sfondo applicato: $wallpaper"
else
    notify "Cambio sfondo fallito"
    log "awww img fallito: $wallpaper"
    exit 1
fi

if [[ -x "$effects_script" ]]; then
    "$effects_script" "$wallpaper" || log "wallpaper_effects fallito: $wallpaper"
else
    log "wallpaper_effects non eseguibile: $effects_script"
fi

notify "Sfondo applicato: $(basename "$wallpaper")"
