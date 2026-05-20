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
    header "Applicazioni"
    row "SUPER Space" "Terminale"
    row "SUPER E" "File manager"
    row "SUPER B" "Browser"
    row "Alt Space" "Launcher applicazioni"
    row "SUPER ." "Emoji"
    row "SUPER V" "Clipboard"
    row "XF86Assistant / F23" "Note"
    printf '\n'

    header "Finestre"
    row "SUPER Q" "Chiudi finestra"
    row "SUPER Shift Q" "Forza chiusura PID"
    row "SUPER Frecce" "Sposta focus"
    row "SUPER Shift Frecce" "Muovi / sposta floating"
    row "SUPER Ctrl Frecce" "Ridimensiona a pixel"
    row "SUPER Mouse sinistro" "Sposta finestra"
    row "SUPER Mouse destro" "Ridimensiona finestra"
    row "SUPER S" "Fullscreen"
    row "SUPER P" "Pseudo tiling"
    row "SUPER J" "Toggle split"
    printf '\n'

    header "Workspace"
    row "SUPER Tab" "Workspace successivo"
    row "SUPER Shift Tab" "Workspace precedente"
    row "SUPER 1..0" "Vai a workspace 1..10"
    row "SUPER Shift 1..0" "Sposta finestra a workspace"
    row "SUPER Scroll" "Scorri workspace"
    printf '\n'

    header "Screenshot e registrazione"
    row "Print" "Screenshot area rapido"
    row "Shift Print" "Screenshot finestra"
    row "Ctrl Print" "Screenshot schermo"
    row "Alt Print" "Menu screenshot"
    row "F9" "Pannello registrazione"
    row "F11" "Pannello screenshot"
    row "SUPER Shift S" "Menu screenshot"
    row "SUPER Shift R" "Menu registrazione"
    printf '\n'

    header "Audio"
    row "XF86 Volume Up/Down" "Volume +/-"
    row "XF86 Mute" "Mute audio"
    row "XF86 MicMute" "Mute microfono"
    row "Control Menu > Audio" "Mixer e dispositivi"
    printf '\n'

    header "Luminosità"
    row "XF86 Brightness +/-" "Luminosità +/-"
    row "SUPER Shift B" "Menu luminosità"
    printf '\n'

    header "Wallpaper"
    row "SUPER W" "Scegli wallpaper"
    row "SUPER Shift W" "Wallpaper casuale"
    printf '\n'

    header "Widget"
    row "SUPER G" "Mostra/nasconde widget"
    row "SUPER Shift G" "Riavvia widget"
    row "SUPER Alt G" "Salva posizione widget"
    row "SUPER Shift Alt G" "Menu widget / aggiungi app"
    row "Tasti volume/luce" "Slider OSD al centro (CSS)"
    row "SUPER Click sx (floating)" "Trascina widget / finestre"
    printf '\n'

    header "Floating mode"
    row "SUPER F" "Toggle floating"
    row "SUPER Shift F" "Floating Manager"
    row "SUPER Ctrl F" "Centra finestra"
    row "SUPER Alt F" "Pin / unpin"
    row "SUPER Ctrl 1/2/3" "Resize small/medium/large"
    row "SUPER Ctrl T" "Porta sopra"
    row "SUPER Ctrl Backspace" "Reset floating"
    printf '\n'

    header "Sistema"
    row "SUPER L" "Blocca schermo"
    row "Power" "Menu spegnimento"
    row "SUPER Shift P" "Proietta schermo"
    row "SUPER Shift Ctrl Esc" "Esci da Hyprland"
    row "SUPER Shift N" "Identifica tasto Copilot"
    printf '\n'

    printf '󱂬 Apri Floating Manager\n'
    printf '󰃠 Apri menu luminosità\n'
    printf '󰍹 Apri Proietta schermo\n'
    printf '󱓞 Toggle widget\n'
    printf '󰈙 Apri keybinding.conf\n'
    printf '󰌍 Indietro\n'
}

choice="$(
    menu_rows |
        rofi -dmenu -i -matching fuzzy \
            -p "Scorciatoie Hyprland" \
            -mesg "Sezioni: Applicazioni, Finestre, Workspace, Screenshot, Audio, Luminosità, Wallpaper, Widget, Floating, Sistema" \
            -theme "$THEME"
)"

case "$choice" in
    *"Floating Manager") "$HOME/.config/anto426/floating_manager.sh" menu ;;
    *"luminosità") "$HOME/.config/anto426/brightness_menu.sh" menu ;;
    *"Proietta") "$HOME/.config/anto426/projection_menu.sh" ;;
    *"Toggle widget") "$HOME/.config/anto426/widgets.sh" toggle ;;
    *"Apri keybinding.conf") xdg-open "$KEYBINDS" >/dev/null 2>&1 & ;;
    *"Indietro") exit 0 ;;
esac
