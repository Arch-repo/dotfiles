#!/usr/bin/env bash
set -uo pipefail

THEME="$HOME/.config/rofi/key_hints.rasi"
KEYBINDS="$HOME/.config/hypr/conf/keybinding.conf"

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

header() {
    printf '── %s ──\n' "$1"
}

row() {
    printf '  %-24s %s\n' "$1" "$2"
}

menu_rows() {
    header "APPLICATIONS"
    row "SUPER Space" "Terminal"
    row "SUPER E" "File Manager"
    row "SUPER B" "Web Browser"
    row "Alt Space" "Application Launcher"
    row "SUPER ." "Emoji Picker"
    row "SUPER V" "Clipboard History"
    row "SUPER N" "Quick Notes"
    printf '\n'

    header "WINDOWS"
    row "SUPER Q" "Close Window"
    row "SUPER Shift Q" "Force Kill Process (PID)"
    row "SUPER Arrows" "Move Focus"
    row "SUPER Shift Arrows" "Move Floating Window"
    row "SUPER Ctrl Arrows" "Resize Window (Pixels)"
    row "SUPER Left Mouse" "Drag / Move Window"
    row "SUPER Right Mouse" "Resize Window"
    row "SUPER S" "Toggle Fullscreen"
    row "SUPER P" "Toggle Pseudo-Tiling"
    row "SUPER J" "Toggle Split (Vertical/Horizontal)"
    printf '\n'

    header "WORKSPACES"
    row "SUPER Tab" "Next Workspace"
    row "SUPER Shift Tab" "Previous Workspace"
    row "SUPER 1..0" "Go to Workspace 1..10"
    row "SUPER Shift 1..0" "Move Window to Workspace 1..10"
    row "SUPER Scroll" "Cycle Workspaces"
    printf '\n'

    header "SCREENSHOTS & RECORDING"
    row "Print" "Quick Screenshot (Area)"
    row "Shift Print" "Screenshot Active Window"
    row "Ctrl Print" "Screenshot Active Screen"
    row "Alt Print" "Screenshot Selector Menu"
    row "F9" "Screen Recording Menu"
    row "F11" "Screenshot Selection Panel"
    row "SUPER Shift S" "Screenshot Menu"
    row "SUPER Shift R" "Recording Menu"
    printf '\n'

    header "AUDIO CONTROLS"
    row "XF86 Volume Up/Down" "Volume Increase / Decrease"
    row "XF86 Mute" "Mute / Unmute Audio"
    row "XF86 MicMute" "Mute / Unmute Microphone"
    row "Control Menu > Audio" "Volume Mixer & Audio Devices"
    printf '\n'

    header "SCREEN BRIGHTNESS"
    row "XF86 Brightness +/-" "Brightness Increase / Decrease"
    row "SUPER Shift B" "Brightness Control Menu"
    printf '\n'

    header "DESKTOP WALLPAPER"
    row "SUPER W" "Select Wallpaper"
    row "SUPER Shift W" "Randomize Wallpaper"
    printf '\n'

    header "DESKTOP WIDGETS"
    row "SUPER G" "Toggle Widget Visibility"
    row "SUPER Shift G" "Restart Desktop Widgets"
    row "SUPER Control G" "Widget Management Menu"
    row "SUPER Alt G" "Save Custom Widget Geometry"
    row "SUPER Shift Alt G" "Visual Presets & Terminal Widgets"
    row "Volume/Brightness Keys" "Center OSD Sliders"
    row "SUPER Left Click (Drag)" "Drag / Move Widget Window"
    printf '\n'

    header "FLOATING WINDOW MODE"
    row "SUPER F" "Toggle Floating Mode"
    row "SUPER Shift F" "Launch Floating Manager"
    row "SUPER Ctrl F" "Center Active Window"
    row "SUPER Alt F" "Pin / Unpin Active Window"
    row "SUPER Ctrl 1/2/3" "Resize Compact/Comfortable/Large"
    row "SUPER Ctrl T" "Bring Window to Front"
    row "SUPER Ctrl Backspace" "Reset Window: Tile + Unpin"
    printf '\n'

    header "SYSTEM ACTIONS"
    row "SUPER L" "Lock Screen"
    row "Power Key" "Shutdown / Power Menu"
    row "SUPER Shift P" "Project Screen (Display Mode)"
    row "SUPER Shift Ctrl Esc" "Exit Hyprland Session"
    printf 'Open Floating Manager\0icon\x1fwindow-restore\n'
    printf 'Open Brightness Menu\0icon\x1fdisplay-brightness\n'
    printf 'Open Project Screen\0icon\x1fvideo-display\n'
    printf 'Toggle Widgets Visibility\0icon\x1fpreferences-desktop-theme\n'
    printf 'Manage Widgets Dashboard\0icon\x1fpreferences-desktop-theme\n'
    printf 'View keybindings.conf\0icon\x1ftext-x-generic\n'
    printf 'Back\0icon\x1fgo-previous\n'
}

while true; do
    choice="$(
        menu_rows |
            rofi -dmenu -i -matching fuzzy -show-icons \
                -p "Hyprland Keybindings" \
                -mesg "Sections: Applications, Windows, Workspaces, Screenshots, Audio, Brightness, Wallpaper, Widgets, Floating, System" \
                -theme "$THEME"
    )"

    case "$choice" in
        "") exit 0 ;;
        "── "* | "  "*) continue ;;
        "Open Floating Manager") "$HOME/.config/anto426/floating_manager.sh" menu; exit 0 ;;
        "Open Brightness Menu") "$HOME/.config/anto426/brightness_menu.sh" menu; exit 0 ;;
        "Open Project Screen") "$HOME/.config/anto426/projection_menu.sh"; exit 0 ;;
        "Toggle Widgets Visibility") "$HOME/.config/anto426/widgets.sh" toggle; exit 0 ;;
        "Manage Widgets Dashboard") "$HOME/.config/anto426/widgets.sh" arrange; exit 0 ;;
        "View keybindings.conf") xdg-open "$KEYBINDS" >/dev/null 2>&1 & exit 0 ;;
        "Back") exit 0 ;;
    esac
done
