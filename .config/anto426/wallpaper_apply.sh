#!/usr/bin/env bash
set -uo pipefail

wallpaper="${1:-}"
transition="${ANTO426_WALLPAPER_TRANSITION:-any}"
duration="${ANTO426_WALLPAPER_DURATION:-2}"
effects_script="$HOME/.config/anto426/wallpaper_effects.sh"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_apply.log"
destination_wallpaper_dir="${XDG_CACHE_HOME:-$HOME/.cache}/awww"
live_options_version="5"
short_loop_threshold="${ANTO426_WALLPAPER_SHORT_LOOP_THRESHOLD:-0}"
short_loop_target="${ANTO426_WALLPAPER_SHORT_LOOP_TARGET:-180}"
short_loop_max_repeats="${ANTO426_WALLPAPER_SHORT_LOOP_MAX_REPEATS:-180}"
short_loop_max_bytes="${ANTO426_WALLPAPER_SHORT_LOOP_MAX_BYTES:-1073741824}"

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

prepared_live_wallpaper() {
    local source="$1"
    local duration source_size repeats hash cache_dir cache_file tmp_file loop_count

    if [[ "$short_loop_threshold" -eq 0 ]]; then
        printf '%s' "$source"
        return 0
    fi

    command -v ffprobe >/dev/null 2>&1 || {
        printf '%s' "$source"
        return 0
    }
    command -v ffmpeg >/dev/null 2>&1 || {
        printf '%s' "$source"
        return 0
    }

    duration="$(ffprobe -v error -show_entries format=duration -of default=nk=1:nw=1 "$source" 2>/dev/null || true)"
    source_size="$(stat -c %s "$source" 2>/dev/null || printf '0')"
    repeats="$(awk \
        -v d="$duration" \
        -v s="$source_size" \
        -v threshold="$short_loop_threshold" \
        -v target="$short_loop_target" \
        -v max_repeats="$short_loop_max_repeats" \
        -v max_bytes="$short_loop_max_bytes" \
        'BEGIN {
        if (threshold <= 0) threshold = 15
        if (target <= 0) target = 180
        if (max_repeats <= 0) max_repeats = 180
        if (max_bytes <= 0) max_bytes = 1073741824

        if (d > 0 && d <= threshold) {
            r = int(target / d)
            if ((target / d) > r) r++
            if (r < 2) r = 2
            if (r > max_repeats) r = max_repeats
            if (s > 0 && (s * r) > max_bytes) {
                by_size = int(max_bytes / s)
                if (by_size < 2) by_size = 2
                if (by_size < r) r = by_size
            }
            print r
        } else {
            print 1
        }
    }')"

    if [[ ! "$repeats" =~ ^[0-9]+$ || "$repeats" -le 1 ]]; then
        printf '%s' "$source"
        return 0
    fi

    cache_dir="$destination_wallpaper_dir/live-loop-cache"
    mkdir -p "$cache_dir"
    hash="$(printf '%s:%s:%s:%s:%s' "$source" "$source_size" "$(stat -c %Y "$source" 2>/dev/null || true)" "$repeats" "$live_options_version" | sha1sum | awk '{print $1}')"
    cache_file="$cache_dir/$hash.mkv"
    if [[ -s "$cache_file" ]]; then
        printf '%s' "$cache_file"
        return 0
    fi

    tmp_file="$cache_dir/$hash.tmp.mkv"
    loop_count=$((repeats - 1))

    if ffmpeg -hide_banner -loglevel error -y \
        -stream_loop "$loop_count" -i "$source" \
        -map 0:v:0 -an -sn -dn -c:v copy -avoid_negative_ts make_zero \
        "$tmp_file" >/dev/null 2>&1; then
        mv "$tmp_file" "$cache_file"
        log "prepared lossless short live wallpaper loop cache: $cache_file (${repeats}x, copy)"
        printf '%s' "$cache_file"
        return 0
    fi

    rm -f "$tmp_file"
    printf '%s' "$source"
}

start_wallpaper_daemon() {
    if ! pgrep -af "[/]wallpaper_daemon([[:space:]]|$)" >/dev/null 2>&1 && ! pgrep -af "[/]wallpaper_daemon.sh" >/dev/null 2>&1; then
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

    saved_live_options_version="$(cat "$destination_wallpaper_dir/current-live-options.version" 2>/dev/null || true)"
    playback_wallpaper="$(prepared_live_wallpaper "$wallpaper")"
    running_live="$(current_live_wallpaper)"
    if [[ "$saved_live_options_version" == "$live_options_version" ]] &&
        pgrep -x mpvpaper >/dev/null 2>&1 &&
        { [[ -n "$running_live" && "$running_live" == "$playback_wallpaper" ]] || [[ -n "$running_live" && "$running_live" == "$wallpaper" ]]; }; then
        log "live wallpaper already active, skipping mpvpaper restart: $wallpaper"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$playback_wallpaper" >"$destination_wallpaper_dir/current-live-playback.path"
        start_wallpaper_daemon
        exit 0
    fi

    # Kill any existing mpvpaper and conflicting daemons
    pkill mpvpaper || true
    pkill awww-daemon || true
    pkill swww-daemon || true

    log "Applying live wallpaper: $wallpaper"
    ipc_sock="${XDG_RUNTIME_DIR:-/tmp}/mpvpaper-ipc"
    if mpvpaper -f -o "no-audio loop-file=inf keep-open=yes --panscan=1.0 --hidpi-window-scale=yes --hwdec=no --osd-level=0 --input-ipc-server=$ipc_sock" '*' "$playback_wallpaper" >"$state_dir/mpvpaper.log" 2>&1; then
        log "live wallpaper applied: $wallpaper"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$playback_wallpaper" >"$destination_wallpaper_dir/current-live-playback.path"
        printf '%s\n' "$live_options_version" >"$destination_wallpaper_dir/current-live-options.version"
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
    ANTO426_WALLPAPER_EFFECTS_EXPECTED="$wallpaper" "$effects_script" "$wallpaper" || log "wallpaper_effects failed: $wallpaper"
else
    log "wallpaper_effects not executable: $effects_script"
fi

notify "Wallpaper applied: $(basename "$wallpaper")"
