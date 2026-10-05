#!/usr/bin/env bash
set -uo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
if [[ "${ANTO426_WALLPAPER_APPLY_IMPL:-c}" != "sh" && -x "$script_dir/wallpaper_core" ]]; then
    exec "$script_dir/wallpaper_core" apply "$@"
fi

wallpaper="${1:-}"
output="${ANTO426_WALLPAPER_OUTPUT:-ALL}"
transition="${ANTO426_WALLPAPER_TRANSITION:-any}"
duration="${ANTO426_WALLPAPER_DURATION:-2}"
skip_effects="${ANTO426_WALLPAPER_SKIP_EFFECTS:-0}"
silent="${ANTO426_WALLPAPER_SILENT:-0}"
effects_script="$HOME/.config/anto426/wallpaper_effects.sh"
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
log_file="$state_dir/wallpaper_apply.log"
destination_wallpaper_dir="${XDG_CACHE_HOME:-$HOME/.cache}/awww"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
local_config_root="${ANTO_LOCAL_CONFIG_ROOT:-$config_home/anto426-local}"
wallpaper_config_dir="$local_config_root/wallpaper"
wallpaper_output_dir="$wallpaper_config_dir/outputs"
live_options_version="5"
short_loop_threshold="${ANTO426_WALLPAPER_SHORT_LOOP_THRESHOLD:-0}"
short_loop_target="${ANTO426_WALLPAPER_SHORT_LOOP_TARGET:-180}"
short_loop_max_repeats="${ANTO426_WALLPAPER_SHORT_LOOP_MAX_REPEATS:-180}"
short_loop_max_bytes="${ANTO426_WALLPAPER_SHORT_LOOP_MAX_BYTES:-1073741824}"

mkdir -p "$state_dir" "$destination_wallpaper_dir" "$wallpaper_config_dir"

case "$output" in
    "" | "__all__" | "*") output="ALL" ;;
esac
if [[ ${#output} -ge 256 || "$output" == *[$'\001'-$'\037'$'\177']* ]]; then
    printf 'Output wallpaper non valido\n' >&2
    exit 2
fi

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

mpvpaper_process_matches() {
    local wanted_output="$1"
    local wanted_wallpaper="$2"
    local pid
    local argument
    local output_matches
    local wallpaper_matches

    while read -r pid; do
        [[ -n "$pid" ]] || continue
        output_matches=0
        wallpaper_matches=0
        while IFS= read -r -d '' argument; do
            if [[ "$wanted_output" == "ALL" ]]; then
                [[ "$argument" == "ALL" || "$argument" == "*" ]] && output_matches=1
            elif [[ "$argument" == "$wanted_output" ]]; then
                output_matches=1
            fi
            if [[ -f "$argument" ]] &&
                [[ "$(readlink -f -- "$argument" 2>/dev/null || printf '%s' "$argument")" == "$wanted_wallpaper" ]]; then
                wallpaper_matches=1
            fi
        done <"/proc/$pid/cmdline" 2>/dev/null || true
        ((output_matches && wallpaper_matches)) && return 0
    done < <(pgrep -x mpvpaper 2>/dev/null || true)
    return 1
}

stop_mpvpaper_for_output() {
    local wanted_output="$1"
    local pid
    local argument
    local matches
    local stopped=0

    while read -r pid; do
        [[ -n "$pid" ]] || continue
        matches=0
        if [[ "$wanted_output" == "ALL" ]]; then
            matches=1
        else
            while IFS= read -r -d '' argument; do
                if [[ "$argument" == "$wanted_output" || "$argument" == "ALL" || "$argument" == "*" ]]; then
                    matches=1
                    break
                fi
            done <"/proc/$pid/cmdline" 2>/dev/null || true
        fi
        if ((matches)) && kill "$pid" 2>/dev/null; then
            stopped=$((stopped + 1))
        fi
    done < <(pgrep -x mpvpaper 2>/dev/null || true)

    if ((stopped > 0)); then
        log "stopped $stopped mpvpaper process(es) for output $wanted_output"
        sleep 0.12
    fi
}

write_output_state() {
    local kind="$1"
    local selected_wallpaper="$2"
    local state_output_dir="$wallpaper_output_dir"
    local state_hash

    if [[ ! -d "$state_output_dir" && -d "$destination_wallpaper_dir/output-state" ]]; then
        mkdir -p "$state_output_dir"
        local legacy_state legacy_output legacy_kind legacy_wallpaper legacy_hash
        for legacy_state in "$destination_wallpaper_dir/output-state"/*.state; do
            [[ -f "$legacy_state" ]] || continue
            {
                IFS= read -r legacy_output
                IFS= read -r legacy_kind
                IFS= read -r legacy_wallpaper
            } <"$legacy_state" || continue
            [[ -n "$legacy_output" && -f "$legacy_wallpaper" ]] || continue
            legacy_hash="$(printf '%s' "$legacy_output" | sha256sum | awk '{print substr($1, 1, 16)}')"
            printf '%s\n%s\n%s\n' "$legacy_output" "$legacy_kind" "$legacy_wallpaper" \
                >"$state_output_dir/$legacy_hash.state"
        done
    fi
    mkdir -p "$state_output_dir"
    if [[ "$output" == "ALL" && "${ANTO426_WALLPAPER_RESTORE_MODE:-0}" != "1" ]]; then
        local stale_state
        for stale_state in "$state_output_dir"/*.state; do
            [[ -e "$stale_state" ]] || continue
            rm -f -- "$stale_state"
        done
    fi
    local existing_state existing_output
    for existing_state in "$state_output_dir"/*.state; do
        [[ -f "$existing_state" ]] || continue
        IFS= read -r existing_output <"$existing_state" || continue
        [[ "$existing_output" == "$output" ]] || continue
        rm -f -- "$existing_state"
    done
    state_hash="$(printf '%s' "$output" | sha256sum | awk '{print substr($1, 1, 16)}')"
    printf '%s\n%s\n%s\n' "$output" "$kind" "$selected_wallpaper" \
        >"$state_output_dir/$state_hash.state"
}

write_current_state() {
    local selected_wallpaper="$1"
    local temporary="$wallpaper_config_dir/.current.state.$$"

    printf '%s\n%s\n' "$output" "$selected_wallpaper" >"$temporary"
    mv -f -- "$temporary" "$wallpaper_config_dir/current.state"
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
    local saved_path saved_output output_state_dir state_file
    local state_output state_kind state_wallpaper
    local status=0
    local -a restore_outputs=()
    local -a restore_wallpapers=()

    if [[ -r "$wallpaper_config_dir/current.state" ]]; then
        {
            IFS= read -r saved_output
            IFS= read -r saved_path
        } <"$wallpaper_config_dir/current.state" || true
    fi
    saved_path="${saved_path:-$(cat "$destination_wallpaper_dir/current-wallpaper.path" 2>/dev/null || true)}"
    saved_output="${saved_output:-$(cat "$destination_wallpaper_dir/current-wallpaper-output.path" 2>/dev/null || printf 'ALL')}"
    output_state_dir="$wallpaper_output_dir"
    if [[ ! -d "$output_state_dir" ]]; then
        output_state_dir="$destination_wallpaper_dir/output-state"
    fi

    for state_file in "$output_state_dir"/*.state; do
        [[ -f "$state_file" ]] || continue
        {
            IFS= read -r state_output
            IFS= read -r state_kind
            IFS= read -r state_wallpaper
        } <"$state_file" || continue
        [[ -n "$state_output" && -f "$state_wallpaper" ]] || continue
        restore_outputs+=("$state_output")
        restore_wallpapers+=("$state_wallpaper")
    done

    if ((${#restore_outputs[@]} == 0)); then
        if [[ -n "$saved_path" && -f "$saved_path" ]]; then
            log "Restoring saved wallpaper: $saved_path on $saved_output"
            exec env ANTO426_WALLPAPER_OUTPUT="$saved_output" "$0" "$saved_path"
        fi
        log "No saved wallpaper to restore"
        exit 0
    fi

    for pass in 0 1; do
        for index in "${!restore_outputs[@]}"; do
            if [[ "$pass" == "0" && "${restore_outputs[$index]}" != "ALL" ]] ||
                [[ "$pass" == "1" && "${restore_outputs[$index]}" == "ALL" ]]; then
                continue
            fi
            env ANTO426_WALLPAPER_OUTPUT="${restore_outputs[$index]}" \
                ANTO426_WALLPAPER_SKIP_EFFECTS=1 \
                ANTO426_WALLPAPER_SILENT=1 \
                ANTO426_WALLPAPER_RESTORE_MODE=1 \
                "$0" "${restore_wallpapers[$index]}" || status=1
        done
    done

    if [[ -n "$saved_path" && -f "$saved_path" ]]; then
        printf '%s\n' "$saved_path" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$saved_output" >"$destination_wallpaper_dir/current-wallpaper-output.path"
        output="$saved_output"
        write_current_state "$saved_path"
        if [[ -x "$effects_script" ]]; then
            ANTO426_WALLPAPER_EFFECTS_EXPECTED="$saved_path" \
                "$effects_script" "$saved_path" ||
                log "wallpaper_effects failed after per-output restore: $saved_path"
        fi
    fi
    log "Restored ${#restore_outputs[@]} wallpaper output state(s)"
    exit "$status"
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
    if [[ "$saved_live_options_version" == "$live_options_version" ]] &&
        mpvpaper_process_matches "$output" "$playback_wallpaper"; then
        log "live wallpaper already active, skipping mpvpaper restart: $wallpaper on $output"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$playback_wallpaper" >"$destination_wallpaper_dir/current-live-playback.path"
        printf '%s\n' "$output" >"$destination_wallpaper_dir/current-wallpaper-output.path"
        write_output_state live "$wallpaper"
        write_current_state "$wallpaper"
        start_wallpaper_daemon
        exit 0
    fi

    # Replace only the selected output. Awww remains alive underneath live
    # layers, so changing one monitor never blanks or resets the others.
    stop_mpvpaper_for_output "$output"
    pkill swww-daemon || true

    log "Applying live wallpaper: $wallpaper on $output"
    output_hash="$(printf '%s' "$output" | sha256sum | awk '{print substr($1, 1, 16)}')"
    ipc_sock="${XDG_RUNTIME_DIR:-/tmp}/mpvpaper-ipc-$output_hash"
    if mpvpaper -f -o "no-audio loop-file=inf keep-open=yes --panscan=1.0 --hidpi-window-scale=yes --hwdec=auto --osd-level=0 --input-ipc-server=$ipc_sock" "$output" "$playback_wallpaper" >"$state_dir/mpvpaper-$output_hash.log" 2>&1; then
        log "live wallpaper applied: $wallpaper on $output"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$playback_wallpaper" >"$destination_wallpaper_dir/current-live-playback.path"
        printf '%s\n' "$output" >"$destination_wallpaper_dir/current-wallpaper-output.path"
        printf '%s\n' "$live_options_version" >"$destination_wallpaper_dir/current-live-options.version"
        write_output_state live "$wallpaper"
        write_current_state "$wallpaper"
        start_wallpaper_daemon
    else
        notify "Errore nell'avvio di mpvpaper"
        log "mpvpaper failed: $wallpaper on $output"
        exit 1
    fi
else
    # It's a static image
    stop_mpvpaper_for_output "$output"
    ensure_awww || exit 1

    output_args=()
    [[ "$output" == "ALL" ]] || output_args=(--outputs "$output")
    if awww img "${output_args[@]}" "$wallpaper" --transition-type "$transition" --transition-duration "$duration"; then
        log "wallpaper applied: $wallpaper on $output"
        printf '%s\n' "$wallpaper" >"$destination_wallpaper_dir/current-wallpaper.path"
        printf '%s\n' "$output" >"$destination_wallpaper_dir/current-wallpaper-output.path"
        write_output_state image "$wallpaper"
        write_current_state "$wallpaper"
    else
        notify "Failed to change wallpaper"
        log "awww img failed: $wallpaper on $output"
        exit 1
    fi
fi

if [[ "$skip_effects" != "1" && -x "$effects_script" ]]; then
    ANTO426_WALLPAPER_EFFECTS_EXPECTED="$wallpaper" "$effects_script" "$wallpaper" || log "wallpaper_effects failed: $wallpaper"
elif [[ "$skip_effects" != "1" ]]; then
    log "wallpaper_effects not executable: $effects_script"
fi

if [[ "$silent" != "1" ]]; then
    notify "Wallpaper applied: $(basename "$wallpaper") · $output"
fi
