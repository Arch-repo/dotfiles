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
RECORD_WRAPPER_FILE="$RUNTIME_DIR/anto426-screen-recording.wrapper"
RECORD_ENCODER_STATE="$RUNTIME_DIR/anto426-screen-recording.encoder"
RECORD_LOG="$STATE_DIR/screen_recording.log"
FILE_MANAGER="${FILE_MANAGER:-nemo}"
CAPTURE_DELAY="${ANTO426_CAPTURE_DELAY:-0.24}"
mode="${1:-menu}"

case "$mode" in
    record-status-icon | record-status-json) ;;
    *)
        if pgrep -x rofi >/dev/null; then
            pkill -x rofi
        fi
        ;;
esac

dismiss_capture_ui() {
    pkill -x rofi 2>/dev/null || true
    pkill -f quickshell 2>/dev/null || true

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

theme_color() {
    local name="$1"
    local fallback="$2"
    local colors="$HOME/.config/colors/colors.sh"

    if [[ -r "$colors" ]]; then
        # shellcheck disable=SC1090
        source "$colors"
    fi

    case "$name" in
        background) printf '%s' "${ANTO426_BACKGROUND:-$fallback}" ;;
        accent) printf '%s' "${ANTO426_ACCENT:-$fallback}" ;;
        select) printf '%s' "${ANTO426_SELECT:-$fallback}" ;;
        *) printf '%s' "$fallback" ;;
    esac
}

slurp_rgba() {
    local color="${1#\#}"
    local alpha="$2"

    if [[ ! "$color" =~ ^[0-9A-Fa-f]{6}$ ]]; then
        color="8cb8e4"
    fi

    printf '%s%s' "$color" "$alpha"
}

slurp_theme_args() {
    local background accent select

    background="$(theme_color background "#000000")"
    accent="$(theme_color accent "#8cb8e4")"
    select="$(theme_color select "$accent")"

    printf '%s\n' -b "$(slurp_rgba "$background" 66)"
    printf '%s\n' -c "$(slurp_rgba "$accent" ff)"
    printf '%s\n' -s "$(slurp_rgba "$select" 55)"
    printf '%s\n' -w 3
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
    rofi -dmenu -i -matching fuzzy -show-icons -p "$1" -theme "$THEME"
}

rofi_pick_msg() {
    local prompt="$1"
    local message="$2"

    message="$(printf '%b' "$message")"
    rofi -dmenu -i -matching fuzzy -show-icons -p "$prompt" -mesg "$message" -theme "$THEME"
}

rofi_input() {
    local prompt="$1"
    local value="${2:-}"
    local message="${3:-}"

    message="$(printf '%b' "$message")"
    printf '%s' "$value" |
        rofi -dmenu -show-icons -p "$prompt" -mesg "$message" -theme "$THEME"
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

ffmpeg_encoder_available() {
    local encoder="$1"

    command -v ffmpeg >/dev/null 2>&1 || return 1
    ffmpeg -hide_banner -encoders 2>/dev/null |
        awk -v encoder="$encoder" '$2 == encoder { found = 1 } END { exit !found }'
}

vaapi_render_device() {
    local configured="${ANTO426_RECORD_VAAPI_DEVICE:-}"
    local device

    if [[ -n "$configured" && -w "$configured" ]]; then
        printf '%s' "$configured"
        return 0
    fi

    for device in /dev/dri/renderD*; do
        [[ -w "$device" ]] || continue
        printf '%s' "$device"
        return 0
    done

    return 1
}

recording_encoder_args() {
    local codec="${ANTO426_RECORD_CODEC:-h264_vaapi}"
    local device

    [[ "${ANTO426_RECORD_HWACCEL:-auto}" == "0" ]] && return 1
    [[ "$codec" == *_vaapi ]] || return 1
    ffmpeg_encoder_available "$codec" || return 1
    device="$(vaapi_render_device)" || return 1

    printf '%s\n' -c "$codec"
    printf '%s\n' -d "$device"
    printf '%s\n' -F 'scale_vaapi=format=nv12:out_range=full'
}

recording_encoder_label() {
    local args=("$@")
    local i

    for ((i = 0; i < ${#args[@]}; i++)); do
        if [[ "${args[$i]}" == "-c" && -n "${args[$((i + 1))]:-}" ]]; then
            printf 'GPU %s' "${args[$((i + 1))]}"
            return 0
        fi
    done

    printf 'CPU default'
}

active_output_name() {
    require_capture_tools hyprctl jq || return 1

    hyprctl monitors -j 2>/dev/null |
        jq -r '.[] | select(.focused == true) | .name' |
        sed -n '1p'
}

geometry_for_mode() {
    local mode="$1"
    local slurp_args=()

    case "$mode" in
        area)
            require_capture_tools slurp || return 1
            sleep 0.12
            mapfile -t slurp_args < <(slurp_theme_args)
            slurp "${slurp_args[@]}"
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

    if [[ -r "$RECORD_PID_FILE" ]]; then
        pid="$(cat "$RECORD_PID_FILE" 2>/dev/null || true)"
        if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
            if [[ "$(ps -p "$pid" -o comm= 2>/dev/null)" == "wf-recorder" ]]; then
                printf '%s' "$pid"
                return 0
            fi
        fi
    fi

    pid="$(pgrep -u "$(id -u)" -x wf-recorder | sed -n '1p' || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]] && kill -0 "$pid" 2>/dev/null; then
        printf '%s\n' "$pid" >"$RECORD_PID_FILE"
        printf '%s' "$pid"
        return 0
    fi

    rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE" "$RECORD_WRAPPER_FILE" "$RECORD_ENCODER_STATE"
    return 1
}

recording_file() {
    cat "$RECORD_FILE_STATE" 2>/dev/null || true
}

recording_encoder() {
    cat "$RECORD_ENCODER_STATE" 2>/dev/null || true
}

recording_status() {
    local pid file encoder

    if pid="$(recording_pid)"; then
        file="$(recording_file)"
        encoder="$(recording_encoder)"
        printf 'Recording Active\nPID: %s\nEncoder: %s\nFile: %s' "$pid" "${encoder:-unknown}" "${file:-unknown}"
    else
        printf 'No active recording'
    fi
}

recording_status_icon() {
    if recording_pid >/dev/null; then
        printf '󰑊\n'
    else
        printf '󰹑\n'
    fi
}

recording_status_json() {
    if recording_pid >/dev/null; then
        printf '{"text":"󰑊","class":"recording","tooltip":"Registrazione attiva"}\n'
    else
        printf '{"text":"󰹑","class":"idle","tooltip":"Cattura"}\n'
    fi
}

launch_recording_process() {
    local file="$1"
    local encoder_label="$2"
    local pid

    shift 2

    (
        child=""
        printf '%s\n' "$BASHPID" >"$RECORD_WRAPPER_FILE"
        trap '[[ -n "$child" ]] && kill -INT "$child" 2>/dev/null; wait "$child" 2>/dev/null; rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE" "$RECORD_WRAPPER_FILE" "$RECORD_ENCODER_STATE"; [[ -s "$file" ]] && notify_file_saved "Recording saved" "$file" "$RECORDING_DIR" "video-x-generic"; exit 0' INT TERM
        printf '[%s] starting wf-recorder (%s):' "$(date '+%F %T')" "$encoder_label" >>"$RECORD_LOG"
        printf ' %q' "$@" >>"$RECORD_LOG"
        printf '\n' >>"$RECORD_LOG"
        wf-recorder "$@" >>"$RECORD_LOG" 2>&1 &
        child=$!
        printf '%s\n' "$child" >"$RECORD_PID_FILE"
        printf '%s\n' "$file" >"$RECORD_FILE_STATE"
        printf '%s\n' "$encoder_label" >"$RECORD_ENCODER_STATE"
        wait "$child"
        status=$?
        rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE" "$RECORD_WRAPPER_FILE" "$RECORD_ENCODER_STATE"

        if [[ -s "$file" ]]; then
            notify_file_saved "Recording saved" "$file" "$RECORDING_DIR" "video-x-generic"
        elif ((status != 0)); then
            notify "Recording failed"
        fi
        exit "$status"
    ) &

    pid=$!
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        recording_pid >/dev/null && return 0
        sleep 0.1
    done

    kill -TERM "$pid" 2>/dev/null || true
    rm -f "$RECORD_PID_FILE" "$RECORD_FILE_STATE" "$RECORD_WRAPPER_FILE" "$RECORD_ENCODER_STATE"
    return 1
}

start_recording() {
    local mode="$1"
    local audio="${2:-false}"
    local pid file geometry output encoder_label
    local args=()
    local capture_args=()
    local encoder_args=()
    local cpu_args=()

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

    case "$mode" in
        area | window)
            geometry="$(geometry_for_mode "$mode")" || return 0
            [[ -n "$geometry" ]] || {
                notify "Selected area not found"
                return 1
            }
            capture_args+=(-g "$geometry")
            ;;
        monitor)
            output="$(active_output_name)"
            if [[ -n "$output" ]]; then
                capture_args+=(-o "$output")
            else
                geometry="$(geometry_for_mode monitor)"
                [[ -n "$geometry" ]] && capture_args+=(-g "$geometry")
            fi
            ;;
        *)
            notify "Invalid recording mode"
            return 1
            ;;
    esac

    mapfile -t encoder_args < <(recording_encoder_args || true)
    args=(-f "$file" "${encoder_args[@]}" "${capture_args[@]}")
    cpu_args=(-f "$file" "${capture_args[@]}")
    [[ "$audio" == "true" ]] && {
        args+=(-a)
        cpu_args+=(-a)
    }

    sleep 0.16

    encoder_label="$(recording_encoder_label "${args[@]}")"
    if launch_recording_process "$file" "$encoder_label" "${args[@]}"; then
        notify_recording_started "$file"
        return 0
    fi

    if ((${#encoder_args[@]} > 0)); then
        printf '[%s] hardware encoder failed to start, retrying CPU default\n' "$(date '+%F %T')" >>"$RECORD_LOG"
        encoder_label="$(recording_encoder_label "${cpu_args[@]}")"
        if launch_recording_process "$file" "$encoder_label" "${cpu_args[@]}"; then
            notify_recording_started "$file"
            return 0
        fi
    fi

    notify "Recording failed to start"
    return 1
}

stop_recording() {
    local pid wrapper

    pid="$(recording_pid)" || {
        notify "No active recording"
        return 0
    }

    notify "Stopping recording..."
    kill -INT "$pid" 2>/dev/null || kill -TERM "$pid" 2>/dev/null || true

    for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        kill -0 "$pid" 2>/dev/null || return 0
        sleep 0.12
    done

    kill -TERM "$pid" 2>/dev/null || true
    sleep 0.25

    if kill -0 "$pid" 2>/dev/null; then
        wrapper="$(cat "$RECORD_WRAPPER_FILE" 2>/dev/null || true)"
        [[ "$wrapper" =~ ^[0-9]+$ ]] && kill -TERM "$wrapper" 2>/dev/null || true
        kill -KILL "$pid" 2>/dev/null || true
    fi
}

pick_action() {
    local stop_line=""
    if recording_pid >/dev/null; then
        stop_line="Stop Recording"$'\0'"icon"$'\x1f'"process-stop"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf 'Screenshot Area\0icon\x1fcamera-photo\n'
        printf 'Screenshot Window\0icon\x1fcamera-photo\n'
        printf 'Screenshot Fullscreen\0icon\x1fcamera-photo\n'
        printf 'Record Selected Area\0icon\x1fcamera-video\n'
        printf 'Record Active Window\0icon\x1fcamera-video\n'
        printf 'Record Active Screen\0icon\x1fcamera-video\n'
        printf 'Record Screen with Audio\0icon\x1fcamera-video\n'
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
        stop_line="Stop Recording"$'\0'"icon"$'\x1f'"process-stop"
    fi

    {
        [[ -n "$stop_line" ]] && printf '%s\n' "$stop_line"
        printf 'Record Selected Area\0icon\x1fcamera-video\n'
        printf 'Record Active Window\0icon\x1fcamera-video\n'
        printf 'Record Active Screen\0icon\x1fcamera-video\n'
        printf 'Record Screen with Audio\0icon\x1fcamera-video\n'
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

waybar_click() {
    if recording_pid >/dev/null; then
        stop_recording
    else
        run_menu
    fi
}

case "$mode" in
    record-status-icon)
        recording_status_icon
        ;;
    record-status-json)
        recording_status_json
        ;;
    waybar-click)
        waybar_click
        ;;
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
