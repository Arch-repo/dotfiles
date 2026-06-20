#!/usr/bin/env bash
set -uo pipefail
export PATH="$HOME/.config/anto426/bin:$PATH"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CORE_BIN="$SCRIPT_DIR/widgets_core"
CONFIG_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets.env"
LAYOUT_FILE="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/widgets_layout.env"
WIDGET_HIDDEN_WORKSPACE="special:anto426-widgets-bg"

focused_monitor_name() {
    hyprctl monitors -j 2>/dev/null | jq -r '.[] | select(.focused == true) | .name' | head -n1
}

pick_widget_monitor() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice monitor

    choice="$(
        {
            printf '%s\n' "Schermo attivo"
            hyprctl monitors -j 2>/dev/null | jq -r '.[] | "\(.name)  \(.width)x\(.height)  @ \(.x),\(.y)"'
        } | rofi -dmenu -i -matching fuzzy -p "Schermo widget" -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    if [[ "$choice" == "Schermo attivo" ]]; then
        monitor="$(focused_monitor_name)"
    else
        monitor="${choice%%  *}"
    fi
    printf '%s' "$monitor"
}

custom_widget_id_from_text() {
    local text="$1"
    local id
    id="$(printf '%s' "$text" | tr '[:upper:]' '[:lower:]' | tr -cs 'a-z0-9._-' '_' | sed -e 's/^_*//' -e 's/_*$//')"
    [[ -n "$id" ]] || id="widget"
    printf '%s' "$id"
}

custom_widget_exists() {
    local id="$1"
    "$CORE_BIN" list-widgets | cut -d'|' -f1 | grep -qFx "$id"
}

custom_widget_unique_id() {
    local base id suffix
    base="$(custom_widget_id_from_text "$1")"
    id="$base"
    suffix=2
    while custom_widget_exists "$id"; do
        id="${base}_${suffix}"
        suffix=$((suffix + 1))
    done
    printf '%s' "$id"
}

preset_widget_rows() {
    command -v cava >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰎈 Spettro audio" \
            "spettro_audio" \
            'cava -p "$HOME/.config/anto426/cava_widget.conf"' \
            620 220
    command -v btop >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰍛 Dashboard sistema" \
            "dashboard_sistema" \
            "btop" \
            760 520
    command -v fastfetch >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰌢 Scheda macchina" \
            "scheda_macchina" \
            'exec bash "$HOME/.config/anto426/widgets.d/scheda_macchina.sh"' \
            620 320
    command -v asciiquarium >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰈺 Acquario ASCII" \
            "acquario_ascii" \
            "asciiquarium -t" \
            620 420
    command -v pipes.sh >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰕮 Tubi animati" \
            "tubi_animati" \
            "pipes.sh -t 2 -r 0 -R" \
            700 360
    command -v cbonsai >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰔱 Bonsai Zen" \
            "bonsai_zen" \
            "cbonsai -l -i -w 12 -L 38 -M 5" \
            540 380
    command -v cmatrix >/dev/null 2>&1 &&
        printf '%s\t%s\t%s\t%s\t%s\n' \
            "󰫐 Pioggia Matrix" \
            "pioggia_matrix" \
            "cmatrix -a -b -u 2" \
            620 360
}

pick_preset_widget() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local choice id_base command width height monitor id class

    choice="$(
        preset_widget_rows | cut -f1 |
            rofi -dmenu -i -matching fuzzy -p "Widget visuale" \
                -mesg "Preset controllati: belli da vedere e gia dimensionati" \
                -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    while IFS=$'\t' read -r label id_base command width height; do
        [[ "$label" == "$choice" ]] || continue
        monitor="$(pick_widget_monitor)" || return 1
        id="$(custom_widget_unique_id "$id_base")"
        class="anto426.widget.cmd.$id"
        "$CORE_BIN" add-custom "$id" "$label" "$class" "$command" terminal "$width" "$height" "$monitor"
        printf '%s' "$id"
        return 0
    done < <(preset_widget_rows)

    return 1
}

prompt_terminal_widget_value() {
    local prompt="$1"
    local message="${2:-}"
    local theme="$HOME/.config/rofi/control_menu.rasi"
    rofi -dmenu -i -p "$prompt" -mesg "$message" -theme "$theme"
}

pick_terminal_widget() {
    local id name command width height class monitor

    name="$(prompt_terminal_widget_value "Nome widget" "Esempio: Processi, Log pacman, Temperatura")"
    [[ -n "$name" ]] || return 1

    command="$(prompt_terminal_widget_value "Comando" "Esempi: btop | watch -n1 sensors | journalctl -f")"
    [[ -n "$command" ]] || return 1

    width="$(prompt_terminal_widget_value "Larghezza" "Invio vuoto = 560")"
    height="$(prompt_terminal_widget_value "Altezza" "Invio vuoto = 320")"
    [[ "$width" =~ ^[0-9]+$ ]] || width=560
    [[ "$height" =~ ^[0-9]+$ ]] || height=320

    monitor="$(pick_widget_monitor)" || return 1
    id="$(custom_widget_unique_id "$name")"
    class="anto426.widget.cmd.$id"

    "$CORE_BIN" add-custom "$id" "$name" "$class" "$command" terminal "$width" "$height" "$monitor"
    printf '%s' "$id"
}

widget_order_default() {
    local saved="" available="" out="" name
    if [[ -f "$LAYOUT_FILE" ]]; then
        saved="$(grep -E '^export ANTO426_WIDGET_ORDER=' "$LAYOUT_FILE" | cut -d'"' -f2)"
    fi
    available="$("$CORE_BIN" list-widgets | cut -d'|' -f1 | tr '\n' ' ')"
    for name in $saved $available; do
        [[ -n "$name" ]] || continue
        case " $available " in
            *" $name "*) ;;
            *) continue ;;
        esac
        case " $out " in
            *" $name "*) ;;
            *) out="${out:+$out }$name" ;;
        esac
    done
    echo "$out"
}

widgets_locked() {
    [[ -f "$CONFIG_FILE" ]] && grep -q '^export ANTO426_WIDGETS_LOCKED=1' "$CONFIG_FILE"
}

widget_label() {
    local id="$1"
    local label
    label="$("$CORE_BIN" list-widgets | grep -E "^${id}\|" | cut -d'|' -f2)"
    printf '%s' "${label:-$id}"
}

widget_client_address() {
    local id="$1"
    local class
    class="$("$CORE_BIN" metadata "$id" ANTO426_WIDGET_CLASS 2>/dev/null)"
    [[ -n "$class" ]] || {
        case "$id" in
            clock) class="anto426.widget.clock" ;;
            cava) class="anto426.widget.cava" ;;
            system) class="anto426.widget.system" ;;
            *) class="anto426.widget.cmd.$id" ;;
        esac
    }
    hyprctl clients -j 2>/dev/null | jq -r --arg class "$class" '.[] | select((.class // "") == $class or (.initialClass // "") == $class) | .address' | head -n1
}

disable_named_widget() {
    local name="$1"
    if [[ "$name" == "clock" || "$name" == "cava" || "$name" == "system" ]]; then
        local current=""
        if [[ -f "$CONFIG_FILE" ]]; then
            current="$(grep '^export ANTO426_WIDGETS_ENABLED=' "$CONFIG_FILE" | cut -d'"' -f2)"
        fi
        local new=""
        for item in $current; do
            [[ "$item" != "$name" ]] && new="${new:+$new }$item"
        done
        local tmp="$(mktemp)"
        awk -v new="$new" '
            /^export ANTO426_WIDGETS_ENABLED=/ {
                print "export ANTO426_WIDGETS_ENABLED=\"" new "\""
                next
            }
            { print }
        ' "$CONFIG_FILE" >"$tmp" && mv "$tmp" "$CONFIG_FILE"
    else
        "$CORE_BIN" remove-custom "$name"
    fi
    local class
    class="$("$CORE_BIN" metadata "$name" ANTO426_WIDGET_CLASS 2>/dev/null)"
    if [[ -n "$class" ]]; then
        hyprctl dispatch closewindow "class:$class" >/dev/null 2>&1
    fi
    "$CORE_BIN" apply-layout
}

enable_builtin_widget_from_menu() {
    local theme="$1"
    local choice name monitor
    choice="$(
        for name in clock cava system; do
            local current=""
            if [[ -f "$CONFIG_FILE" ]]; then
                current="$(grep '^export ANTO426_WIDGETS_ENABLED=' "$CONFIG_FILE" | cut -d'"' -f2)"
            fi
            case " $current " in
                *" $name "*) continue ;;
            esac
            printf '%s\n' "$(widget_label "$name")"
        done | rofi -dmenu -i -p "Add base widget" -theme "$theme"
    )"
    [[ -n "$choice" ]] || return 1

    case "$choice" in
        *Clock*) name="clock" ;;
        *Cava*) name="cava" ;;
        *System*) name="system" ;;
        *) return 1 ;;
    esac

    local current=""
    if [[ -f "$CONFIG_FILE" ]]; then
        current="$(grep '^export ANTO426_WIDGETS_ENABLED=' "$CONFIG_FILE" | cut -d'"' -f2)"
    fi
    local new="${current:+$current }$name"
    local tmp="$(mktemp)"
    awk -v new="$new" '
        /^export ANTO426_WIDGETS_ENABLED=/ {
            print "export ANTO426_WIDGETS_ENABLED=\"" new "\""
            next
        }
        { print }
    ' "$CONFIG_FILE" >"$tmp" && mv "$tmp" "$CONFIG_FILE"

    "$CORE_BIN" apply-layout
}

move_widget_in_order() {
    local name="$1"
    local direction="$2"
    local order idx count i tmp new_order
    local -a items

    order="$(widget_order_default)"
    read -ra items <<<"$order"
    count="${#items[@]}"
    for ((i = 0; i < count; i++)); do
        [[ "${items[i]}" == "$name" ]] && idx=$i && break
    done

    case "$direction" in
        up)
            [[ -n "${idx:-}" && "$idx" -gt 0 ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx - 1]}"
            items[idx - 1]="$tmp"
            ;;
        down)
            [[ -n "${idx:-}" && "$idx" -lt $((count - 1)) ]] || return 0
            tmp="${items[idx]}"
            items[idx]="${items[idx + 1]}"
            items[idx + 1]="$tmp"
            ;;
        *)
            return 1
            ;;
    esac

    new_order="${items[*]}"
    local tmp_file="$(mktemp)"
    awk -v order="$new_order" '
        /^export ANTO426_WIDGET_ORDER=/ {
            print "export ANTO426_WIDGET_ORDER=\"" order "\""
            next
        }
        { print }
    ' "$LAYOUT_FILE" >"$tmp_file" && mv "$tmp_file" "$LAYOUT_FILE"

    "$CORE_BIN" apply-layout
    notify-send "Widget" "Ordine aggiornato" 2>/dev/null || true
}

widget_submenu() {
    local name="$1"
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local label choice

    label="$(widget_label "$name")"

    choice="$(
        {
            if "$CORE_BIN" is-running "$name" >/dev/null; then
                printf '  󰖭  Hide temporarily\n'
            else
                printf '  󰖯  Show widget\n'
            fi
            printf '  󰏬  Move Up (Change order)\n'
            printf '  󰏏  Move Down (Change order)\n'
            printf '  󰆴  Remove permanently\n'
        } | rofi -dmenu -i -p "$label" -theme "$theme"
    )"

    [[ -z "$choice" ]] && { arrange_widgets; return 0; }

    case "$choice" in
        *"Hide temporarily"*)
            local addr
            addr="$(widget_client_address "$name")"
            if [[ -n "$addr" ]]; then
                hyprctl dispatch movetoworkspacesilent "$WIDGET_HIDDEN_WORKSPACE,address:$addr" >/dev/null 2>&1 || true
                notify-send "Widget" "$label hidden" 2>/dev/null || true
            fi
            arrange_widgets
            ;;
        *"Show widget"*)
            local addr monitor active_ws
            addr="$(widget_client_address "$name")"
            if [[ -n "$addr" ]]; then
                monitor="$(focused_monitor_name)"
                active_ws="$(hyprctl monitors -j 2>/dev/null | jq -r --arg name "$monitor" '.[] | select(.name == $name) | .activeWorkspace.id')"
                hyprctl dispatch movetoworkspacesilent "${active_ws:-1},address:$addr" >/dev/null 2>&1 || true
                notify-send "Widget" "$label shown" 2>/dev/null || true
            else
                "$CORE_BIN" start
            fi
            arrange_widgets
            ;;
        *"Move Up"*)
            move_widget_in_order "$name" up
            widget_submenu "$name"
            ;;
        *"Move Down"*)
            move_widget_in_order "$name" down
            widget_submenu "$name"
            ;;
        *"Remove permanently"*)
            disable_named_widget "$name"
            notify-send "Widget" "$label removed" 2>/dev/null || true
            arrange_widgets
            ;;
    esac
}

arrange_widgets() {
    local theme="$HOME/.config/rofi/control_menu.rasi"
    local order choice name new_id selected selected_widget status_running

    status_running="$("$CORE_BIN" status)"
    order="$(widget_order_default)"

    choice="$(
        {
            if [[ "$status_running" == "running" ]]; then
                printf 'Stop Widgets\0icon\x1fprocess-stop\n'
            else
                printf 'Start Widgets\0icon\x1fmedia-play\n'
            fi
            printf 'Restart Daemons\0icon\x1fview-refresh\n'
            if widgets_locked; then
                printf 'Unlock Widget Positions\0icon\x1fsystem-lock-screen\n'
            else
                printf 'Lock Widget Positions\0icon\x1fsystem-lock-screen\n'
            fi
            
            printf 'Save Current Layout\0icon\x1fdocument-save\n'
            printf 'Restore Saved Layout\0icon\x1fdocument-revert\n'
            printf 'Reset Default Geometries\0icon\x1fedit-clear\n'
            
            printf 'Visual Presets (Cava, Fastfetch, etc.)\0icon\x1futilities-terminal\n'
            printf 'Custom Terminal Command\0icon\x1fsystem-run\n'
            printf 'Enable Base Widget (Clock, etc.)\0icon\x1foffice-calendar\n'
            
            printf 'Select Widget to Remove\0icon\x1fedit-delete\n'
            
            if [[ -n "$order" ]]; then
                for name in $order; do
                    [[ -n "$name" ]] || continue
                    local label
                    label="$(widget_label "$name")"
                    [[ -n "$label" ]] && printf 'Manage Widget: %s (%s)\0icon\x1fpreferences-system-windows\n' "$label" "$name"
                done
            fi
        } | rofi -dmenu -i -p "Widget Dashboard" \
            -mesg "Premium interface to manage desktop widgets and system layouts" -theme "$theme"
    )"

    [[ -z "$choice" ]] && return 0

    case "$choice" in
        *"Start Widgets"* | *"Stop Widgets"*)
            if [[ "$status_running" == "running" ]]; then
                "$CORE_BIN" stop
            else
                "$CORE_BIN" start
            fi
            arrange_widgets
            ;;
        *"Restart Daemons"*)
            "$CORE_BIN" stop
            sleep 0.3
            "$CORE_BIN" start
            arrange_widgets
            ;;
        *"Lock Widget Positions"*) "$CORE_BIN" lock; arrange_widgets ;;
        *"Unlock Widget Positions"*) "$CORE_BIN" unlock; arrange_widgets ;;
        *"Save Current Layout"*) "$CORE_BIN" save-layout; arrange_widgets ;;
        *"Restore Saved Layout"*) "$CORE_BIN" apply-layout; arrange_widgets ;;
        *"Reset Default Geometries"*)
            rm -f "$LAYOUT_FILE"
            "$CORE_BIN" apply-layout
            arrange_widgets
            ;;
        *"Visual Presets"*)
            if new_id="$(pick_preset_widget)"; then
                "$CORE_BIN" apply-layout
                notify-send "Widget" "Visual widget added" 2>/dev/null || true
            fi
            arrange_widgets
            ;;
        *"Custom Terminal Command"*)
            if new_id="$(pick_terminal_widget)"; then
                "$CORE_BIN" apply-layout
                notify-send "Widget" "Custom command widget added" 2>/dev/null || true
            fi
            arrange_widgets
            ;;
        *"Enable Base Widget"*)
            enable_builtin_widget_from_menu "$theme"
            arrange_widgets
            ;;
        *"Select Widget to Remove"*)
            choice_widgets="$(for name in $order; do printf '%s\n' "$(widget_label "$name")"; done | rofi -dmenu -i -p "Remove Widget" -theme "$theme")"
            if [[ -n "$choice_widgets" ]]; then
                for name in $order; do
                    if [[ "$(widget_label "$name")" == "$choice_widgets" ]]; then
                        disable_named_widget "$name"
                        notify-send "Widget" "Widget removed" 2>/dev/null || true
                        break
                    fi
                done
            fi
            arrange_widgets
            ;;
        *"Manage Widget: "*)
            selected_widget="$(printf '%s' "$choice" | sed -E 's/.*\(([^)]+)\)/\1/')"
            if [[ -n "$selected_widget" ]]; then
                widget_submenu "$selected_widget"
            fi
            ;;
    esac
}

case "${1:-toggle}" in
    autostart)
        # Check autostart flag in env file
        if grep -q '^export ANTO426_WIDGETS_AUTOSTART=0' "$CONFIG_FILE" 2>/dev/null; then
            exit 0
        fi
        "$CORE_BIN" start
        ;;
    start) "$CORE_BIN" start ;;
    stop) "$CORE_BIN" stop ;;
    restart|reload)
        "$CORE_BIN" stop
        sleep 0.3
        "$CORE_BIN" start
        ;;
    toggle) "$CORE_BIN" toggle ;;
    save-layout) "$CORE_BIN" save-layout ;;
    arrange) arrange_widgets ;;
    lock) "$CORE_BIN" lock ;;
    unlock) "$CORE_BIN" unlock ;;
    toggle-lock) "$CORE_BIN" toggle-lock ;;
    lock-daemon) "$CORE_BIN" daemon ;;
    status) "$CORE_BIN" status ;;
    *)
        printf 'Uso: %s [autostart|start|stop|restart|reload|toggle|save-layout|arrange|lock|unlock|toggle-lock|status]\n' "$0" >&2
        exit 2
        ;;
esac
