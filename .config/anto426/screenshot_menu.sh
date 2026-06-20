#!/usr/bin/env bash
set -uo pipefail
export PATH="$HOME/.config/anto426/bin:$PATH"

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

notify_file_saved() {
    local title="$1"
    local file="$2"
    local folder="$3"
    local icon="$4"

    [[ -n "$file" ]] || {
        notify "$title"
        return 0
    }

    (
        local action
        action="$(
            notify-send \
                -a "Capture" \
                -i "$icon" \
                -A "default=Open Folder" \
                -A "file=Open File" \
                "$title" \
                "$file" 2>/dev/null || true
        )"

        case "$action" in
            default)
                open_folder "$folder" >/dev/null 2>&1 &
                ;;
            file)
                xdg-open "$file" >/dev/null 2>&1 &
                ;;
        esac
    ) &
}

notify_recording_started() {
    local file="$1"

    (
        local action
        action="$(
            notify-send \
                -a "Capture" \
                -i "media-record" \
                -A "default=Open Folder" \
                -A "stop=Stop" \
                "Recording started" \
                "$file" 2>/dev/null || true
        )"

        case "$action" in
            default)
                open_folder "$RECORDING_DIR" >/dev/null 2>&1 &
                ;;
            stop)
                "$0" record-stop >/dev/null 2>&1 &
                ;;
        esac
    ) &
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

require_capture_tools() {
    local cmd

    for cmd in "$@"; do
        command -v "$cmd" >/dev/null 2>&1 || {
            notify "$cmd not found"
            return 1
        }
    done
}

active_output_name() {
    require_capture_tools hyprctl jq || return 1

    hyprctl monitors -j 2>/dev/null |
        jq -r '.[] | select(.focused == true) | .name' |
        sed -n '1p'
}

geometry_for_mode() {
    local mode="$1"

    case "$mode" in
        area)
            require_capture_tools slurp || return 1
            sleep 0.12
            slurp -b 00000066 -c 8cb8e4ff -s 8cb8e433 -w 2
            ;;
        window)
            require_capture_tools hyprctl jq || return 1
            hyprctl activewindow -j 2>/dev/null |
                jq -r 'select(.at != null and .size != null) | "\(.at[0]),\(.at[1]) \(.size[0])x\(.size[1])"'
            ;;
        monitor)
            require_capture_tools hyprctl jq || return 1
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
            notify_file_saved "Screenshot saved" "$file" "$dir" "image-x-generic"
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
            notify_file_saved "Screenshot saved" "$file" "$(dirname "$file")" "image-x-generic"
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
        notify_file_saved "Screenshot saved" "$file" "$(dirname "$file")" "image-x-generic"
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
        trap '[[ -n "$child" ]] && kill -INT "$child" 2>/dev/null; wait "$child" 2>/dev/null; rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE"; [[ -s "$file" ]] && notify_file_saved "Recording saved" "$file" "$RECORDING_DIR" "video-x-generic"; exit 0' INT TERM
        wf-recorder "${args[@]}" >>"$RECORD_LOG" 2>&1 &
        child=$!
        wait "$child"
        status=$?
        rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE"

        if [[ -s "$file" ]]; then
            notify_file_saved "Recording saved" "$file" "$RECORDING_DIR" "video-x-generic"
        elif ((status != 0)); then
            notify "Recording failed"
        fi
        exit "$status"
    ) &

    pid=$!
    printf '%s\n' "$pid" > "$RECORD_PID_FILE"
    printf '%s\n' "$file" > "$RECORD_FILE_STATE"
    notify_recording_started "$file"
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
        stop_line="Stop Recording"$'\0'"icon"$'\x1f'"media-playback-stop"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf 'Screenshot Area\0icon\x1fcamera-photo\n'
        printf 'Screenshot Window\0icon\x1fcamera-photo\n'
        printf 'Screenshot Fullscreen\0icon\x1fcamera-photo\n'
        printf 'Record Selected Area\0icon\x1fmedia-record\n'
        printf 'Record Active Window\0icon\x1fmedia-record\n'
        printf 'Record Active Screen\0icon\x1fmedia-record\n'
        printf 'Record Screen with Audio\0icon\x1fmedia-record\n'
        printf 'Open Screenshots Folder\0icon\x1ffolder-pictures\n'
        printf 'Open Recordings Folder\0icon\x1ffolder-videos\n'
        if ! command -v wf-recorder >/dev/null 2>&1 && command -v obs >/dev/null 2>&1; then
            printf 'Open OBS Studio\0icon\x1fobs\n'
        fi
        printf 'Back\0icon\x1fgo-previous\n'
    } | rofi_pick_msg "Capture" "$(recording_status)"
}

pick_record_action() {
    local stop_line=""
    if recording_pid >/dev/null; then
        stop_line="Stop Recording"$'\0'"icon"$'\x1f'"media-playback-stop"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf 'Record Selected Area\0icon\x1fmedia-record\n'
        printf 'Record Active Window\0icon\x1fmedia-record\n'
        printf 'Record Active Screen\0icon\x1fmedia-record\n'
        printf 'Record Screen with Audio\0icon\x1fmedia-record\n'
        printf 'Open Recordings Folder\0icon\x1ffolder-videos\n'
        if ! command -v wf-recorder >/dev/null 2>&1 && command -v obs >/dev/null 2>&1; then
            printf 'Open OBS Studio\0icon\x1fobs\n'
        fi
        printf 'Back\0icon\x1fgo-previous\n'
    } | rofi_pick_msg "Record" "$(recording_status)"
}

pick_screenshot_action() {
    {
        printf 'Screenshot Area\0icon\x1fcamera-photo\n'
        printf 'Screenshot Window\0icon\x1fcamera-photo\n'
        printf 'Screenshot Fullscreen\0icon\x1fcamera-photo\n'
        printf 'Open Screenshots Folder\0icon\x1ffolder-pictures\n'
        printf 'Back\0icon\x1fgo-previous\n'
    } |
        rofi_pick_msg "Screenshot" "Folder: $SCREENSHOT_DIR"
}

run_menu() {
    local choice

    choice="$(pick_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == "Back" ]]; then
        return 0
    fi

    case "$choice" in
        "Stop Recording") stop_recording ;;
        "Screenshot Area")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot area "$(default_screenshot_path)"
            ;;
        "Screenshot Window")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot window "$(default_screenshot_path)"
            ;;
        "Screenshot Fullscreen")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot monitor "$(default_screenshot_path)"
            ;;
        "Record Selected Area") start_recording area false ;;
        "Record Active Window") start_recording window false ;;
        "Record Active Screen") start_recording monitor false ;;
        "Record Screen with Audio") start_recording monitor true ;;
        "Open Screenshots Folder")
            open_folder "$SCREENSHOT_DIR"
            ;;
        "Open Recordings Folder")
            open_folder "$RECORDING_DIR"
            ;;
        "Open OBS Studio")
            obs >/dev/null 2>&1 &
            ;;
    esac
}

run_screenshot_menu() {
    local choice

    choice="$(pick_screenshot_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == "Back" ]]; then
        return 0
    fi

    case "$choice" in
        "Screenshot Area")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot area "$(default_screenshot_path)"
            ;;
        "Screenshot Window")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot window "$(default_screenshot_path)"
            ;;
        "Screenshot Fullscreen")
            mkdir -p "$SCREENSHOT_DIR"
            save_screenshot monitor "$(default_screenshot_path)"
            ;;
        "Open Screenshots Folder") open_folder "$SCREENSHOT_DIR" ;;
    esac
}

run_record_menu() {
    local choice

    choice="$(pick_record_action)"
    [[ -n "$choice" ]] || return 0
    if [[ "$choice" == "Back" ]]; then
        return 0
    fi

    case "$choice" in
        "Stop Recording") stop_recording ;;
        "Record Selected Area") start_recording area false ;;
        "Record Active Window") start_recording window false ;;
        "Record Active Screen") start_recording monitor false ;;
        "Record Screen with Audio") start_recording monitor true ;;
        "Open Recordings Folder") open_folder "$RECORDING_DIR" ;;
        "Open OBS Studio") obs >/dev/null 2>&1 & ;;
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
