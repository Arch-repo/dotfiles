#!/usr/bin/env bash
set -uo pipefail

ipc_socket="${XDG_RUNTIME_DIR:-/tmp}/mpvpaper-ipc"
hypr_signature="${HYPRLAND_INSTANCE_SIGNATURE:-}"
hypr_socket=""
[[ -n "$hypr_signature" ]] && hypr_socket="/tmp/hypr/$hypr_signature/.socket2.sock"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_daemon.log"

mkdir -p "$state_dir"

log() {
    printf '[%s] %s\n' "$(date '+%F %T')" "$*" >>"$log_file"
}

pause_wallpaper() {
    if [[ -S "$ipc_socket" ]]; then
        echo '{"command": ["set_property", "pause", true]}' | nc -N -U "$ipc_socket" >/dev/null 2>&1 || true
    fi
}

resume_wallpaper() {
    if [[ -S "$ipc_socket" ]]; then
        echo '{"command": ["set_property", "pause", false]}' | nc -N -U "$ipc_socket" >/dev/null 2>&1 || true
    fi
}

update_state() {
    # 1. Controlla se mpvpaper è in esecuzione
    if ! pgrep -x mpvpaper >/dev/null 2>&1; then
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

# Controllo iniziale all'avvio del daemon
update_state

# Ascolta gli eventi di Hyprland in tempo reale per reagire all'istante
if [[ -n "$hypr_socket" && -S "$hypr_socket" ]] && command -v nc >/dev/null 2>&1; then
    nc -U "$hypr_socket" 2>/dev/null | while read -r line; do
        case "$line" in
            "fullscreen>>"* | "workspace>>"* | "openwindow>>"* | "closewindow>>"* | "activewindow>>"* | "changefloatingmode>>"*)
                update_state
                ;;
        esac
    done
else
    # Fallback tramite polling in caso di problemi con il socket
    while true; do
        update_state
        sleep 2
    done
fi
