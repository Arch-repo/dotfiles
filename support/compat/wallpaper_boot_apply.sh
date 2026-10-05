#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
wallpaper_core="${ANTO426_WALLPAPER_CORE:-$script_dir/wallpaper_core}"
commit_script="${ANTO426_WALLPAPER_BOOT_COMMIT_SCRIPT:-$script_dir/wallpaper_boot_commit.sh}"
asset="${1:-}"
if [[ "$asset" == "--current" ]]; then
    asset="$(cat "${XDG_CACHE_HOME:-$HOME/.cache}/awww/current-wallpaper.path" 2>/dev/null || true)"
fi

if [[ -z "$asset" || ! -f "$asset" ]]; then
    printf 'Uso: %s /percorso/wallpaper | --current\n' "$0" >&2
    exit 2
fi

if [[ ! -x "$wallpaper_core" ]]; then
    printf 'Backend Avvio e login non disponibile: %s\n' "$wallpaper_core" >&2
    exit 1
fi
if [[ ! -x "$commit_script" ]]; then
    printf 'Commit Avvio e login non disponibile: %s\n' "$commit_script" >&2
    exit 1
fi

# Serialize rendering and installation together; desktop and explicit boot
# selections must never interleave the assets of two generations.
state_dir="${XDG_STATE_HOME:-$HOME/.local/state}/anto426"
mkdir -p -- "$state_dir"
exec 9>"$state_dir/boot-login-apply.lock"
flock -x 9

# Fail closed with older cores: never fall back to regular `apply`, which
# would unexpectedly replace the live desktop wallpaper.
if ! "$wallpaper_core" --help 2>/dev/null | grep -Fq 'wallpaper_core boot'; then
    printf 'Il backend non supporta ancora il comando boot-only: %s boot <asset>\n' \
        "$wallpaper_core" >&2
    exit 1
fi

generation="$($wallpaper_core boot "$asset")"
generation="$(tail -n 1 <<<"$generation")"
[[ -n "$generation" ]] || {
    printf 'Il backend non ha prodotto una generazione valida\n' >&2
    exit 1
}
exec "$commit_script" "$generation"
