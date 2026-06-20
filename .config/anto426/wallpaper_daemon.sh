#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ "${ANTO426_WALLPAPER_DAEMON_IMPL:-c}" != "sh" && -x "$script_dir/wallpaper_daemon" ]]; then
    exec "$script_dir/wallpaper_daemon"
fi

ipc_socket="${XDG_RUNTIME_DIR:-/tmp}/mpvpaper-ipc"
hypr_signature="${HYPRLAND_INSTANCE_SIGNATURE:-}"
hypr_socket=""
if [[ -n "$hypr_signature" ]]; then
    hypr_socket="${XDG_RUNTIME_DIR:-/tmp}/hypr/$hypr_signature/.socket2.sock"
    [[ -S "$hypr_socket" ]] || hypr_socket="/tmp/hypr/$hypr_signature/.socket2.sock"
fi
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_daemon.log"
wallpaper_auto_pause="${ANTO426_WALLPAPER_AUTO_PAUSE:-0}"

mkdir -p "$state_dir"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

wallpaper_paused=""
last_wallpaper_command_ms=0

now_ms() {
    date '+%s%3N'
}

set_wallpaper_pause() {
    local desired="$1"
    local now elapsed wait_ms

    [[ "$wallpaper_paused" == "$desired" ]] && return 0
    [[ -S "$ipc_socket" ]] || return 0

    now="$(now_ms)"
    if [[ "$last_wallpaper_command_ms" =~ ^[0-9]+$ && "$last_wallpaper_command_ms" -gt 0 ]]; then
        elapsed=$((now - last_wallpaper_command_ms))
        if (( elapsed < 300 )); then
            wait_ms=$((300 - elapsed))
            sleep "0.$(printf '%03d' "$wait_ms")"
            now="$(now_ms)"
        fi
    fi

    if [[ "$desired" == "1" ]]; then
        echo '{"command": ["set_property", "pause", true]}' | nc -N -U "$ipc_socket" >/dev/null 2>&1 || true
    else
        echo '{"command": ["set_property", "pause", false]}' | nc -N -U "$ipc_socket" >/dev/null 2>&1 || true
    fi
    wallpaper_paused="$desired"
    last_wallpaper_command_ms="$now"
}

pause_wallpaper() {
    if [[ -S "$ipc_socket" ]]; then
        set_wallpaper_pause 1
    fi
}

resume_wallpaper() {
    if [[ -S "$ipc_socket" ]]; then
        set_wallpaper_pause 0
    fi
}

auto_pause_enabled() {
    case "${wallpaper_auto_pause,,}" in
        0 | false | no | off) return 1 ;;
        *) return 0 ;;
    esac
}

update_state() {
    # 1. Controlla se mpvpaper è in esecuzione
    if ! pgrep -x mpvpaper >/dev/null 2>&1; then
        return
    fi

    if ! auto_pause_enabled; then
        resume_wallpaper
        return
    fi

    # 2. Controlla se la schermata è bloccata con hyprlock
    if pgrep -x hyprlock >/dev/null 2>&1; then
        pause_wallpaper
        return
    fi

    # 3. Ottieni le informazioni sul workspace attivo
    local workspace_json
    workspace_json="$(hyprctl activeworkspace -j 2>/dev/null || true)"
    if [[ -z "$workspace_json" ]]; then
        return
    fi

    # Se c'è una finestra a schermo intero, metti in pausa
    if [[ "$(printf '%s' "$workspace_json" | jq -r '.hasfullscreen' 2>/dev/null || echo "false")" == "true" ]]; then
        pause_wallpaper
        return
    fi

    # 4. Ottieni il nome del workspace attivo
    local active_ws
    active_ws="$(printf '%s' "$workspace_json" | jq -r '.name' 2>/dev/null || echo "")"
    if [[ -n "$active_ws" ]]; then
        # Conta le finestre affiancate (tiled, non-floating) sul workspace attivo
        local tiled_count
        tiled_count="$(hyprctl clients -j 2>/dev/null | jq -r --arg ws "$active_ws" 'map(select(.workspace.name == $ws and .floating == false)) | length' 2>/dev/null || echo "0")"
        if (( tiled_count > 0 )); then
            pause_wallpaper
            return
        fi
    fi

    # 5. Se il workspace è vuoto o contiene solo finestre fluttuanti, riprendi la riproduzione
    resume_wallpaper
}

while true; do
    update_state

    if ! auto_pause_enabled; then
        sleep 30
        continue
    fi

    if [[ -n "$hypr_socket" && -S "$hypr_socket" ]] && command -v nc >/dev/null 2>&1; then
        while read -r line; do
            case "$line" in
                "fullscreen>>"* | "workspace>>"* | "openwindow>>"* | "closewindow>>"* | "activewindow>>"* | "changefloatingmode>>"*)
                    update_state
                    ;;
            esac
        done < <(nc -U "$hypr_socket" 2>/dev/null)
        sleep 1
    else
        sleep 2
    fi
done
