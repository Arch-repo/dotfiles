#!/usr/bin/env bash
set -uo pipefail

notify-send "Copilot key" "Premi il tasto Copilot nella finestra wev e leggi keysym/keycode." 2>/dev/null || true

if ! command -v wev >/dev/null 2>&1; then
    notify-send "Copilot key" "Installa wev per identificare il tasto." 2>/dev/null || true
    exit 1
fi

if command -v ghostty >/dev/null 2>&1; then
    exec ghostty --class=copilot-key-detector --title="Copilot key detector" -e wev
fi

exec wev
