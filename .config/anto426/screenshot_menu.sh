#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"
SCREENSHOT_DIR="$HOME/Pictures/Screenshots"
RECORDING_DIR="$HOME/Videos/Recordings"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
RECORD_PID_FILE="$RUNTIME_DIR/anto426-screen-recording.pid"
RECORD_FILE_STATE="$RUNTIME_DIR/anto426-screen-recording.file"
RECORD_LOG="$STATE_DIR/screen_recording.log"
FILE_MANAGER="${FILE_MANAGER:-nemo}"
CAPTURE_DELAY="${ANTO426_CAPTURE_DELAY:-0.24}"

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

dismiss_capture_ui() {
    pkill -x rofi 2>/dev/null || true

    if [[ "$CAPTURE_DELAY" =~ ^([0-9]+([.][0-9]+)?|[.][0-9]+)$ ]]; then
        sleep "$CAPTURE_DELAY"
    else
        sleep 0.24
    fi
}

notify() {
    notify-send "Capture" "$*" 2>/dev/null || true
}

desktop_background_color() {
    local colors="$HOME/.config/colors/colors.sh"

    if [[ -r "$colors" ]]; then
        # shellcheck disable=SC1090
        source "$colors"
    fi

    printf '%s' "${ANTO426_BACKGROUND:-#000000}"
}

finalize_screenshot() {
    local file="$1"
    local background

    [[ -s "$file" ]] || return 1
    command -v magick >/dev/null 2>&1 || return 0

    background="$(desktop_background_color)"
    magick "$file" -background "$background" -alpha remove -alpha off "$file" 2>/dev/null || true
}

open_folder() {
    local path="$1"
    local opener

    mkdir -p "$path"

    if command -v "$FILE_MANAGER" >/dev/null 2>&1; then
        "$FILE_MANAGER" "$path" >/dev/null 2>&1 &
        return 0
    fi

    for opener in nemo dolphin thunar nautilus pcmanfm; do
        if command -v "$opener" >/dev/null 2>&1; then
            "$opener" "$path" >/dev/null 2>&1 &
            return 0
        fi
    done

    notify "File manager not found"
    return 1
}

rofi_pick() {
    rofi -dmenu -i -matching fuzzy -p "$1" -theme "$THEME"
}

rofi_pick_msg() {
    local prompt="$1"
    local message="$2"

    message="$(printf '%b' "$message")"
    rofi -dmenu -i -matching fuzzy -p "$prompt" -mesg "$message" -theme "$THEME"
}

rofi_input() {
    local prompt="$1"
    local value="${2:-}"
    local message="${3:-}"

    message="$(printf '%b' "$message")"
    printf '%s' "$value" |
        rofi -dmenu -p "$prompt" -mesg "$message" -theme "$THEME"
}

timestamp() {
    date +%Y-%m-%d_%H-%M-%S
}

screenshot_filename() {
    printf 'screenshot-%s.png' "$(timestamp)"
}

default_screenshot_path() {
    printf '%s/%s' "$SCREENSHOT_DIR" "$(screenshot_filename)"
}

recording_filename() {
    printf 'recording-%s.mkv' "$(timestamp)"
}

normalize_image_path() {
    local path="$1"

    case "$path" in
        "~/"*) path="$HOME/${path#~/}" ;;
        /*) ;;
        *) path="$SCREENSHOT_DIR/$path" ;;
    esac

    [[ "$path" == *.png ]] || path="$path.png"
    printf '%s' "$path"
}

pick_image_file() {
    local filename file

    mkdir -p "$SCREENSHOT_DIR"
    filename="$(screenshot_filename)"
    file="$(rofi_input "Screenshot name" "$filename" "Folder: $SCREENSHOT_DIR")"
    [[ -n "$file" ]] || return 1

    normalize_image_path "$file"
}

active_output_name() {
    hyprctl monitors -j 2>/dev/null |
        jq -r '.[] | select(.focused == true) | .name' |
        sed -n '1p'
}

geometry_for_mode() {
    local mode="$1"

    case "$mode" in
        area)
            sleep 0.12
            slurp -b 00000066 -c 8cb8e4ff -s 8cb8e433 -w 2
            ;;
        window)
            hyprctl activewindow -j 2>/dev/null |
                jq -r 'select(.at != null and .size != null) | "\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"'
            ;;
        monitor)
            hyprctl monitors -j 2>/dev/null |
                jq -r '.[] | select(.focused == true) | "\(.x),\(.y) \(.width)x\(.height)"' |
                sed -n '1p'
            ;;
    esac
}

hyprshot_mode_args() {
    case "$1" in
        area) printf '%s\n' -z -m region ;;
        window) printf '%s\n' -z -m window ;;
        monitor) printf '%s\n' -m output -m active ;;
        *) return 1 ;;
    esac
}

save_screenshot_hyprshot() {
    local mode="$1"
    local file="$2"
    local dir base
    local args=()

    command -v hyprshot >/dev/null 2>&1 || return 1

    dir="$(dirname "$file")"
    base="$(basename "$file")"
    mapfile -t args < <(hyprshot_mode_args "$mode")

    if hyprshot "${args[@]}" -o "$dir" -f "$base" --silent >/dev/null 2>&1; then
        if [[ -s "$file" ]]; then
            finalize_screenshot "$file"
            wl-copy < "$file" 2>/dev/null || true
            notify "Screenshot saved: $file"
            return 0
        fi
    fi

    return 1
}

save_screenshot_grim() {
    local mode="$1"
    local file="$2"
    local geometry output

    command -v grim >/dev/null 2>&1 || {
        notify "Install grim to take screenshots"
        return 1
    }

    if [[ "$mode" == "monitor" ]]; then
        output="$(active_output_name)"
        if [[ -n "$output" ]] && grim -o "$output" "$file"; then
            finalize_screenshot "$file"
            wl-copy < "$file" 2>/dev/null || true
            notify "Screenshot saved: $file"
            return 0
        fi
    fi

    geometry="$(geometry_for_mode "$mode")" || return 0
    if [[ -z "$geometry" ]]; then
        notify "Selected area not found"
        return 1
    fi

    if grim -g "$geometry" "$file"; then
        finalize_screenshot "$file"
        wl-copy < "$file" 2>/dev/null || true
        notify "Screenshot saved: $file"
        return 0
    fi

    notify "Screenshot failed"
    return 1
}

save_screenshot() {
    local mode="$1"
    local file="$2"

    dismiss_capture_ui
    mkdir -p "$(dirname "$file")"

    if [[ "$mode" == "area" || "$mode" == "monitor" ]]; then
        save_screenshot_grim "$mode" "$file"
        return
    fi

    save_screenshot_hyprshot "$mode" "$file" && return 0
    save_screenshot_grim "$mode" "$file"
}

recording_pid() {
    local pid

    [[ -r "$RECORD_PID_FILE" ]] || return 1
    pid="$(cat "$RECORD_PID_FILE" 2>/dev/null || true)"
    [[ "$pid" =~ ^[0-9]+$ ]] || return 1

    if kill -0 "$pid" 2>/dev/null; then
        printf '%s' "$pid"
        return 0
    fi

    rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE"
    return 1
}

recording_file() {
    cat "$RECORD_FILE_STATE" 2>/dev/null || true
}

recording_status() {
    local pid file

    if pid="$(recording_pid)"; then
        file="$(recording_file)"
        printf 'Recording Active\nPID: %s\nFile: %s' "$pid" "${file:-unknown}"
    else
        printf 'No active recording'
    fi
}

start_recording() {
    local mode="$1"
    local audio="${2:-false}"
    local pid file geometry output
    local args=()

    command -v wf-recorder >/dev/null 2>&1 || {
        notify "wf-recorder not found. Please install it."
        return 1
    }

    dismiss_capture_ui

    if pid="$(recording_pid)"; then
        notify "Recording already active: PID $pid"
        return 0
    fi

    mkdir -p "$RECORDING_DIR" "$STATE_DIR"
    file="$RECORDING_DIR/$(recording_filename)"
    args=(-f "$file")

    case "$mode" in
        area | window)
            geometry="$(geometry_for_mode "$mode")" || return 0
            [[ -n "$geometry" ]] || {
                notify "Selected area not found"
                return 1
            }
            args+=(-g "$geometry")
            ;;
        monitor)
            output="$(active_output_name)"
            if [[ -n "$output" ]]; then
                args+=(-o "$output")
            else
                geometry="$(geometry_for_mode monitor)"
                [[ -n "$geometry" ]] && args+=(-g "$geometry")
            fi
            ;;
        *)
            notify "Invalid recording mode"
            return 1
            ;;
    esac

    [[ "$audio" == "true" ]] && args+=(-a)
    sleep 0.16

    (
        child=""
        trap '[[ -n "$child" ]] && kill -INT "$child" 2>/dev/null; wait "$child" 2>/dev/null; rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE"; [[ -s "$file" ]] && notify "Recording saved: $file"; exit 0' INT TERM
        wf-recorder "${args[@]}" >>"$RECORD_LOG" 2>&1 &
        child=$!
        wait "$child"
        status=$?
        rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE"

        if [[ -s "$file" ]]; then
            notify "Recording saved: $file"
        elif ((status != 0)); then
            notify "Recording failed"
        fi
        exit "$status"
    ) &

    pid=$!
    printf '%s\n' "$pid" > "$RECORD_PID_FILE"
    printf '%s\n' "$file" > "$RECORD_FILE_STATE"
    notify "Recording started: $(basename "$file")"
}

stop_recording() {
    local pid

    pid="$(recording_pid)" || {
        notify "No active recording"
        return 0
    }

    kill -INT "$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true
    notify "Stopping recording..."
}

pick_action() {
    local stop_line=""
    if recording_pid >/dev/null; then
        stop_line="󰓛  Stop Recording"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf '󰇄 SCREENSHOT ACTIONS\n'
        printf '%s\n' \
            " ├─ 󰆞  Screenshot Area" \
            " ├─ 󰖲  Screenshot Window" \
            " └─ 󰍹  Screenshot Fullscreen"
        printf '\n󰒔 SCREEN RECORDING\n'
        printf '%s\n' \
            " ├─ 󰑊  Record Selected Area" \
            " ├─ 󰑊  Record Active Window" \
            " ├─ 󰑊  Record Active Screen" \
            " └─ 󰕾  Record Screen with Audio"
        printf '\n󰥔 DIRECTORIES & TOOLS\n'
        printf '%s\n' \
            " ├─   Open Screenshots Folder" \
            " └─   Open Recordings Folder"
        if ! command -v wf-recorder >/dev/null 2>&1 && command -v obs >/dev/null 2>&1; then
            printf '\n󰐌  Studio Tools\n'
            printf '%s\n' " └─ 󰐌  Open OBS Studio"
        fi
        printf '\n󰌍  Back\n'
    } | rofi_pick_msg "Capture" "$(recording_status)"
}

pick_record_action() {
    local stop_line=""
    if recording_pid >/dev/null; then
        stop_line="󰓛  Stop Recording"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf '󰒔 SCREEN RECORDING\n'
        printf '%s\n' \
            " ├─ 󰑊  Record Selected Area" \
            " ├─ 󰑊  Record Active Window" \
            " ├─ 󰑊  Record Active Screen" \
            " ├─ 󰕾  Record Screen with Audio" \
            " └─   Open Recordings Folder"
        if ! command -v wf-recorder >/dev/null 2>&1 && command -v obs >/dev/null 2>&1; then
            printf '\n󰐌  Studio Tools\n'
            printf '%s\n' " └─ 󰐌  Open OBS Studio"
        fi
        printf '\n󰌍  Back\n'
    } | rofi_pick_msg "Record" "$(recording_status)"
}

pick_screenshot_action() {
    {
        printf '󰇄 SCREENSHOT ACTIONS\n'
        printf '%s\n' \
            " ├─ 󰆞  Screenshot Area" \
            " ├─ 󰖲  Screenshot Window" \
            " ├─ 󰍹  Screenshot Fullscreen" \
            " └─   Open Screenshots Folder"
        printf '\n󰌍  Back\n'
    } |
        rofi_pick_msg "Screenshot" "Folder: $SCREENSHOT_DIR"
}

run_menu() {
    local choice

    choice="$(pick_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == *"Back"* ]]; then
        return 0
    fi

    case "$choice" in
        "󰇄"* | "󰒔"* | "󰥔"* | "󰌍"*) run_menu ;;
        *"Stop Recording" | *"Ferma registrazione"*) stop_recording ;;
        *"Screenshot Area" | *"Screenshot area"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot area "$(default_screenshot_path)"
            ;;
        *"Screenshot Window" | *"Screenshot finestra"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot window "$(default_screenshot_path)"
            ;;
        *"Screenshot Fullscreen" | *"Screenshot schermo intero"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot monitor "$(default_screenshot_path)"
            ;;
        *"Record Selected Area" | *"Registra area"*) start_recording area false ;;
        *"Record Active Window" | *"Registra finestra"*) start_recording window false ;;
        *"Record Active Screen" | *"Registra schermo attivo"*) start_recording monitor false ;;
        *"Record Screen with Audio" | *"Registra schermo + audio"*) start_recording monitor true ;;
        *"Open Screenshots Folder" | *"Apri screenshot"*)
            open_folder "$SCREENSHOT_DIR"
            ;;
        *"Open Recordings Folder" | *"Apri registrazioni"*)
            open_folder "$RECORDING_DIR"
            ;;
        *"Open OBS Studio" | *"Apri OBS Studio"*)
            obs >/dev/null 2>&1 &
            ;;
        *"Back" | *"Indietro"*)
            return 0
            ;;
    esac
}

run_screenshot_menu() {
    local choice

    choice="$(pick_screenshot_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == *"Back"* ]]; then
        return 0
    fi

    case "$choice" in
        "󰇄"* | "󰌍"*) run_screenshot_menu ;;
        *"Screenshot Area" | *"Screenshot area"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot area "$(default_screenshot_path)"
            ;;
        *"Screenshot Window" | *"Screenshot finestra"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot window "$(default_screenshot_path)"
            ;;
        *"Screenshot Fullscreen" | *"Screenshot schermo intero"*)
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot monitor "$(default_screenshot_path)"
            ;;
        *"Open Screenshots Folder" | *"Apri screenshot"*) open_folder "$SCREENSHOT_DIR" ;;
        *"Back" | *"Indietro"*) return 0 ;;
    esac
}

run_record_menu() {
    local choice

    choice="$(pick_record_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == *"Back"* ]]; then
        return 0
    fi

    case "$choice" in
        "󰒔"* | "󰌍"*) run_record_menu ;;
        *"Stop Recording" | *"Ferma registrazione"*) stop_recording ;;
        *"Record Selected Area" | *"Registra area"*) start_recording area false ;;
        *"Record Active Window" | *"Registra finestra"*) start_recording window false ;;
        *"Record Active Screen" | *"Registra schermo attivo"*) start_recording monitor false ;;
        *"Record Screen with Audio" | *"Registra schermo + audio"*) start_recording monitor true ;;
        *"Open Recordings Folder" | *"Apri registrazioni"*) open_folder "$RECORDING_DIR" ;;
        *"Open OBS Studio" | *"Apri OBS Studio"*) obs >/dev/null 2>&1 & ;;
        *"Back" | *"Indietro"*) return 0 ;;
    esac
}

mode="${1:-menu}"
case "$mode" in
    menu)
        run_menu
        ;;
    screenshot-menu)
        run_screenshot_menu
        ;;
    quick-area)
        mkdir -p "$SCREENSHOT_DIR"
        save_screenshot area "$(default_screenshot_path)"
        ;;
    quick-window)
        mkdir -p "$SCREENSHOT_DIR"
        save_screenshot window "$(default_screenshot_path)"
        ;;
    quick-monitor)
        mkdir -p "$SCREENSHOT_DIR"
        save_screenshot monitor "$(default_screenshot_path)"
        ;;
    record-menu)
        run_record_menu
        ;;
    record-area)
        start_recording area false
        ;;
    record-window)
        start_recording window false
        ;;
    record-monitor)
        start_recording monitor false
        ;;
    record-monitor-audio)
        start_recording monitor true
        ;;
    record-stop)
        stop_recording
        ;;
    *)
        run_menu
        ;;
esac
