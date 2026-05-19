#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/control_menu.rasi"
KEYBINDS="$HOME/.config/hypr/conf/keybinding.conf"

if pgrep -x rofi > /dev/null; then
    pkill -x rofi
fi

choice="$(
    printf '%s\n' \
        "  =      SUPER KEY (Windows key)" \
        "  H       Show keybinding hints" \
        "  Space   Open terminal" \
        "  E       Open file manager" \
        "  B       Open browser" \
        "  Shift Ctrl Esc   Exit Hyprland" \
        "  Q       Close active window" \
        "  Shift Q Kill active window by PID" \
        "  F       Toggle floating" \
        "  P       Toggle pseudo (dwindle)" \
        "  J       Toggle split (dwindle)" \
        "  L       Lock screen" \
        "Alt Space  App launcher" \
        "  .       Emoji selector" \
        "  V       Clipboard manager" \
        "  W       Choose wallpaper" \
        "  Shift W Random wallpaper" \
        "  Shift S Screenshot menu" \
        "  Shift R Screen recorder menu" \
        "Print      Screenshot menu" \
        "Shift Print Quick screenshot region" \
        "Alt Print  Quick screenshot window" \
        "Ctrl Print Recorder menu" \
        "Power      Power menu" \
        "  Tab     Next workspace" \
        "  Shift Tab Previous workspace" \
        "  Shift Arrows Move active window" \
        "  Ctrl Arrows Resize active window" \
        "  [1 -> 0] Switch workspace 1-10" \
        "  Shift [1 -> 0] Move window to workspace 1-10" \
        "󰈙 Apri keybinding.conf" |
        rofi -dmenu -i -matching fuzzy -p "Tasti Hyprland" -theme "$THEME"
)"

case "$choice" in
    *"Apri keybinding.conf")
        xdg-open "$KEYBINDS" >/dev/null 2>&1 &
        ;;
esac
