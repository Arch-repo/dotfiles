#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/rofi.sh
source "$SCRIPT_DIR/lib/rofi.sh"

anto426_close_rofi

choice="$(cliphist list | anto426_rofi_dmenu "Clipboard" "$ANTO426_THEME_CONTROL")"
[[ -n "$choice" ]] || exit 0

printf '%s' "$choice" | cliphist decode | wl-copy
