#!/usr/bin/env bash
set -uo pipefail

CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/anto426/media"
MARQUEE_WIDTH="${ANTO426_MEDIA_MARQUEE_WIDTH:-34}"
PLAYER_PID_FILE="${XDG_RUNTIME_DIR:-/tmp}/anto426-media-player.pid"

json() {
    jq -cn \
        --arg text "${1:-}" \
        --arg tooltip "${2:-}" \
        --arg class "${3:-}" \
        '{text: $text, tooltip: $tooltip, class: $class}'
}

notify() {
    notify-send "Media" "$*" 2>/dev/null || true
}

metadata() {
    playerctl metadata "$1" 2>/dev/null || true
}

metadata_format() {
    playerctl metadata --format "$1" 2>/dev/null || true
}

escape_markup() {
    local value="${1:-}"
    value="${value//&/\&amp;}"
    value="${value//</\&lt;}"
    value="${value//>/\&gt;}"
    ESCAPED_MARKUP="$value"
}

status_icon() {
    local player="$1"
    local status="$2"
    local icon

    case "${player,,}" in
        spotify*) icon="" ;;
        firefox*) icon="󰈹" ;;
        chrome*|chromium*) icon="" ;;
        mpv*) icon="" ;;
        *) icon="" ;;
    esac

    case "${status,,}" in
        paused) icon="" ;;
    esac

    STATUS_ICON="$icon"
}

youtube_id_from_url() {
    local url="$1"
    local id=""

    case "$url" in
        *"youtube.com/watch"*"?v="*|*"youtube.com/watch"*"&v="*)
            id="${url#*v=}"
            id="${id%%[&?/#]*}"
            ;;
        *"youtu.be/"*)
            id="${url#*youtu.be/}"
            id="${id%%[&?/#]*}"
            ;;
        *"youtube.com/shorts/"*)
            id="${url#*youtube.com/shorts/}"
            id="${id%%[&?/#]*}"
            ;;
    esac

    if [[ "$id" =~ ^[A-Za-z0-9_-]{6,}$ ]]; then
        printf '%s' "$id"
    fi
}

cache_key() {
    printf '%s' "$1" | sha256sum | awk '{print $1}'
}

download_cover() {
    local url="$1"
    local key file tmp

    mkdir -p "$CACHE_DIR"
    key="$(cache_key "$url")"
    file="$CACHE_DIR/$key.jpg"
    tmp="$file.tmp"

    if [[ ! -s "$file" ]] && command -v curl >/dev/null 2>&1; then
        if curl -LfsS --max-time 4 "$url" -o "$tmp" 2>/dev/null; then
            mv "$tmp" "$file"
        else
            rm -f "$tmp"
        fi
    fi

    [[ -s "$file" ]] && printf '%s' "$file"
}

decode_file_uri() {
    local path="${1#file://}"
    path="${path%%\?*}"
    printf '%b' "${path//%/\\x}"
}

placeholder_cover() {
    local file="$CACHE_DIR/placeholder.png"
    local colors="${ANTO_LOCAL_CONFIG_ROOT:-${XDG_CONFIG_HOME:-$HOME/.config}/anto426-local}/theme/colors.sh"
    local bg="#2b2434"
    local select="#8e647a"
    local accent="#db81aa"

    mkdir -p "$CACHE_DIR"
    if [[ -r "$colors" ]]; then
        # shellcheck disable=SC1090
        source "$colors"
        bg="${ANTO426_BACKGROUND:-$bg}"
        select="${ANTO426_SELECT:-$select}"
        accent="${ANTO426_ACCENT:-$accent}"
    fi

    if [[ ! -s "$file" ]] && command -v magick >/dev/null 2>&1; then
        magick -size 256x256 "xc:$bg" \
            -fill "$select" -draw "roundrectangle 18,18 238,238 34,34" \
            -fill "$accent" -draw "circle 128,128 128,76" \
            -fill "$bg" -draw "circle 128,128 128,104" \
            PNG32:"$file" 2>/dev/null || true
    fi

    [[ -s "$file" ]] && printf '%s' "$file"
}

cover_path() {
    local art_url="$1"
    local page_url="$2"
    local path video_id

    if [[ "$art_url" == file://* ]]; then
        path="$(decode_file_uri "$art_url")"
        [[ -r "$path" ]] && {
            printf '%s' "$path"
            return 0
        }
    elif [[ "$art_url" == http://* || "$art_url" == https://* ]]; then
        path="$(download_cover "$art_url")"
        [[ -n "$path" ]] && {
            printf '%s' "$path"
            return 0
        }
    fi

    video_id="$(youtube_id_from_url "$page_url")"
    if [[ -n "$video_id" ]]; then
        path="$(download_cover "https://img.youtube.com/vi/$video_id/hqdefault.jpg")"
        [[ -n "$path" ]] && {
            printf '%s' "$path"
            return 0
        }
    fi

    placeholder_cover
}

progress_line() {
    local pos_raw length_raw pos length width filled empty

    pos_raw="$(playerctl position 2>/dev/null || printf '0')"
    length_raw="$(metadata mpris:length)"
    [[ "$length_raw" =~ ^[0-9]+$ ]] || length_raw=0

    awk -v pos="$pos_raw" -v length_us="$length_raw" '
        BEGIN {
            length = length_us / 1000000
            width = 18
            if (length <= 0) {
                print "──────────────────"
                exit
            }
            filled = int((pos / length) * width + 0.5)
            if (filled < 0) filled = 0
            if (filled > width) filled = width
            for (i = 0; i < filled; i++) printf "━"
            for (i = filled; i < width; i++) printf "─"
            print ""
        }'
}

marquee_text() {
    local text="$1"
    local width="${2:-$MARQUEE_WIDTH}"
    local spacer="   •   "
    local loop offset len now_us tick

    [[ "$width" =~ ^[0-9]+$ ]] || width=34
    (( width < 12 )) && width=12

    if (( ${#text} <= width )); then
        MARQUEE_TEXT="$text"
        return 0
    fi

    loop="${text}${spacer}${text}${spacer}"
    len=$((${#text} + ${#spacer}))
    now_us="${EPOCHREALTIME/./}"
    if [[ "$now_us" =~ ^[0-9]+$ && ${#now_us} -gt 10 ]]; then
        tick=$((now_us / 160000))
    else
        printf -v now_us '%(%s)T' -1
        tick=$((now_us * 6))
    fi
    offset=$((tick % len))
    MARQUEE_TEXT="${loop:offset:width}"
}

status_json() {
    local player status title artist icon class text tooltip label

    if ! command -v playerctl >/dev/null 2>&1; then
        json "" "playerctl non installato" "missing"
        return 0
    fi

    if ! IFS=$'\x1f' read -r player status title artist < <(
        playerctl metadata --format $'{{playerName}}\x1f{{status}}\x1f{{title}}\x1f{{artist}}' 2>/dev/null
    ); then
        json "" "" "empty"
        return 0
    fi
    [[ -n "$player" ]] || {
        json "" "" "empty"
        return 0
    }

    status="${status:-Unknown}"
    status_icon "$player" "$status"
    icon="$STATUS_ICON"

    case "${status,,}" in
        playing) class="playing" ;;
        paused) class="paused" ;;
        *) class="stopped" ;;
    esac

    if [[ -n "${title:-}" && -n "${artist:-}" ]]; then
        label="$icon $title - $artist"
    elif [[ -n "${title:-}" ]]; then
        label="$icon $title"
    else
        label="$icon ${player:-Media}"
    fi
    marquee_text "$label"
    escape_markup "$MARQUEE_TEXT"
    text="$ESCAPED_MARKUP"

    tooltip="${player:-Player}
${status:-Unknown}"
    [[ -n "${artist:-}" ]] && tooltip+="
$artist"
    [[ -n "${title:-}" ]] && tooltip+="
$title"

    escape_markup "$tooltip"
    tooltip="$ESCAPED_MARKUP"
    json "$text" "$tooltip" "$class"
}

button_json() {
    local direction="$1"
    local icon tooltip

    if ! command -v playerctl >/dev/null 2>&1 || ! playerctl status >/dev/null 2>&1; then
        json "" "" "empty"
        return 0
    fi

    if [[ "$direction" == "previous" ]]; then
        icon="󰒮"
        tooltip="Traccia precedente"
    else
        icon="󰒭"
        tooltip="Traccia successiva"
    fi
    json "$icon" "$tooltip" "media-control"
}

watch_status() {
    local delay="${ANTO426_MEDIA_WATCH_INTERVAL:-0.5}"

    while true; do
        status_json
        sleep "$delay"
    done
}

media_menu() {
    exec "$HOME/.local/bin/anto-menu" audio
}

case "${1:-status}" in
    status | waybar | json)
        status_json
        ;;
    watch)
        watch_status
        ;;
    button-previous)
        button_json previous
        ;;
    button-next)
        button_json next
        ;;
    menu)
        media_menu
        ;;
    play-pause)
        playerctl play-pause 2>/dev/null || true
        ;;
    previous)
        playerctl previous 2>/dev/null || true
        ;;
    next)
        playerctl next 2>/dev/null || true
        ;;
    *)
        status_json
        ;;
esac
