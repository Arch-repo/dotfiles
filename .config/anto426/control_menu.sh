#!/usr/bin/env bash
set -uo pipefail

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export ANTO426_MENU_DIR="$SCRIPT_DIR/control_menu.d"

# finite state machine runner
MENU_STATE="${1:-main}"

while [[ -n "$MENU_STATE" ]]; do
    case "$MENU_STATE" in
        bluetooth|wifi|audio|battery|keyboard|notifications|calendar|power|main|control)
            # Sourcing it allows it to modify MENU_STATE to transition back or to other menus!
            current_state="$MENU_STATE"
            MENU_STATE=""
            source "$ANTO426_MENU_DIR/${current_state/control/main}.sh"
            ;;
        *)
            MENU_STATE=""
            ;;
    esac
done
