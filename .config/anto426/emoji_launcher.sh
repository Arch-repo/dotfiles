#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib/rofi.sh
source "$SCRIPT_DIR/lib/rofi.sh"

anto426_close_rofi

rofi -show emoji -theme "$ANTO426_THEME_LAUNCHER"
