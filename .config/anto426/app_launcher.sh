#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/rofi.sh
source "$SCRIPT_DIR/lib/rofi.sh"

anto426_close_rofi

rofi -show drun \
    -show-icons \
    -icon-theme "${ANTO426_ICON_THEME:-Anto426-Material}" \
    -application-fallback-icon "application-default-icon" \
    -p "Apps" \
    -matching fuzzy \
    -sort \
    -sorting-method fzf \
    -theme "$ANTO426_THEME_LAUNCHER"
