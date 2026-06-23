#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

xkb_rules_file() {
    if [[ -r /usr/share/X11/xkb/rules/base.lst ]]; then
        printf '/usr/share/X11/xkb/rules/base.lst'
    else
        printf '/usr/share/X11/xkb/rules/evdev.lst'
    fi
}

xkb_layout_codes() {
    local configured
    configured="$(
        awk -F= '
            /^[[:space:]]*kb_layout[[:space:]]*=/ {
                gsub(/[[:space:]]/, "", $2)
                print $2
                exit
            }
        ' "$HOME/.config/hypr/conf/input.conf" 2>/dev/null
    )"

    [[ -n "$configured" ]] || configured="$(localectl status 2>/dev/null | awk -F: '/X11 Layout:/ {gsub(/^[[:space:]]+/, "", $2); print $2; exit}')"
    printf '%s\n' "${configured:-it}" | tr ',' '\n' | awk 'NF && !seen[$0]++ {print}'
}

xkb_raw_description() {
    local code="$1"
    awk -v code="$code" '
        /^! layout/ {in_layout = 1; next}
        /^!/ {in_layout = 0}
        in_layout && $1 == code {
            $1 = ""
            sub(/^[[:space:]]+/, "")
            print
            exit
        }
    ' "$(xkb_rules_file)" 2>/dev/null
}

xkb_description() {
    local code="$1"
    local raw locale_name
    raw="$(xkb_raw_description "$code")"
    raw="${raw:-$code}"
    locale_name="$UI_LOCALE"

    if command -v gettext >/dev/null 2>&1; then
        LC_ALL="$locale_name" gettext -d xkeyboard-config "$raw" 2>/dev/null || printf '%s' "$raw"
    else
        printf '%s' "$raw"
    fi
}

keyboard_active_keymap() {
    if command -v jq >/dev/null 2>&1; then
        hyprctl devices -j 2>/dev/null |
            jq -r '.keyboards[]? | select(.main == true) | .active_keymap // empty' |
            sed -n '1p'
    else
        hyprctl devices 2>/dev/null | awk -F': ' '/active keymap:/ {print $2; exit}'
    fi
}

keyboard_active_code() {
    local active="$1"
    local code raw translated

    while IFS= read -r code; do
        raw="$(xkb_raw_description "$code")"
        translated="$(xkb_description "$code")"
        if [[ "$active" == "$raw" || "$active" == "$translated" ]]; then
            printf '%s' "$code"
            return 0
        fi
    done < <(xkb_layout_codes)

    case "$active" in
        *Italian*) printf 'it' ;;
        *"English (US)"* | *English*) printf 'us' ;;
        *) xkb_layout_codes | sed -n '1p' ;;
    esac
}

keyboard_rows() {
    local active_code="$1"
    local code index active_suffix label desc icon
    index=0

    while IFS= read -r code; do
        active_suffix=""
        icon="input-keyboard"
        if [[ "$code" == "$active_code" ]]; then
            active_suffix=" ($(system_text "Active"))"
            icon="emblem-default"
        fi
        label="$(printf '%s' "$code" | tr '[:lower:]' '[:upper:]')"
        desc="$(xkb_description "$code")"
        local entry_label="$desc (${label})$active_suffix"
        printf '%s\t%s\t%s\t%s\t%s\n' "$entry_label" "$index" "$code" "$desc" "$icon"
        index=$((index + 1))
    done < <(xkb_layout_codes)
}

keyboard_menu() {
    while true; do
        local active active_code rows choice selected index code desc im_status gtk_status
        local title next_layout configure diagnostics
        active="$(keyboard_active_keymap)"
        active_code="$(keyboard_active_code "$active")"
        rows="$(keyboard_rows "$active_code")"
        im_status="fcitx5 $(system_text "Inactive")"
        pgrep -x fcitx5 >/dev/null 2>&1 && im_status="fcitx5 $(system_text "Active")"
        gtk_status="GTK_IM_MODULE $(system_text "Unset")"
        [[ -n "${GTK_IM_MODULE:-}" ]] && gtk_status="GTK_IM_MODULE=$GTK_IM_MODULE"
        title="$(system_text "Keyboard")"

        choice="$(
            {
                printf '%s\0icon\x1fmedia-playlist-next\n' "$(menu_item "󰌌" "Switch to Next Layout")"
                printf '%s\0icon\x1fmedia-playlist-previous\n' "$(menu_item "󰌌" "Switch to Previous Layout")"
                
                if [[ -n "$rows" ]]; then
                    while IFS=$'\t' read -r label index code desc icon; do
                        [[ -n "$code" ]] || continue
                        printf '%s\0icon\x1f%s\n' "$label" "${icon:-input-keyboard}"
                    done <<< "$rows"
                fi
                
                if command -v fcitx5-configtool >/dev/null 2>&1; then
                    printf '%s\0icon\x1fpreferences-desktop-keyboard\n' "$(menu_item "󰌌" "Input Method Settings")"
                fi
                if command -v fcitx5 >/dev/null 2>&1; then
                    printf '%s\0icon\x1fview-refresh\n' "$(menu_item "󰑐" "Restart Input Method")"
                fi
                printf '%s\0icon\x1futilities-system-monitor\n' "$(menu_item "󰄧" "Diagnostics")"
                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "$title" "<b>$(system_text "Current")</b>: <b><span foreground='${c_accent}'>$(xkb_description "$active_code")</span></b>\n<b>$(system_text "System")</b>: <span foreground='${c_muted}'>${im_status}</span>\n<span foreground='${c_muted}'>${gtk_status}</span>"
        )"

        [[ -z "$choice" ]] && return 0
        local clean_choice="$choice"

        case "$clean_choice" in
            "$(system_text "Switch to Next Layout")")
                run_or_notify "$(system_text "Keyboard")" hyprctl switchxkblayout all next
                ;;
            "$(system_text "Switch to Previous Layout")")
                run_or_notify "$(system_text "Keyboard")" hyprctl switchxkblayout all prev
                ;;
            "$(system_text "Input Method Settings")")
                open_or_notify "Input Method Settings" fcitx5-configtool
                return 0
                ;;
            "$(system_text "Restart Input Method")")
                pkill -x fcitx5 >/dev/null 2>&1 || true
                fcitx5 >/dev/null 2>&1 &
                notify "fcitx5 riavviato"
                ;;
            "$(system_text "Diagnostics")")
                rofi_pick_msg "Diagnostics" "$(localectl status 2>/dev/null)\n\nHyprland: ${active:-$(system_text "Unknown")}\n$im_status\n$gtk_status" >/dev/null
                ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
            *)
                selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$clean_choice" '$1 == chosen {print; exit}')"
                [[ -n "$selected" ]] || continue
                index="$(printf '%s' "$selected" | awk -F'\t' '{print $2}')"
                code="$(printf '%s' "$selected" | awk -F'\t' '{print $3}')"
                desc="$(printf '%s' "$selected" | awk -F'\t' '{print $4}')"
                if hyprctl switchxkblayout all "$index" >/dev/null 2>&1; then
                    notify "$(system_text "Keyboard"): $desc"
                else
                    notify "$(system_text "Keyboard") $(system_text "Failed"): $code"
                fi
                ;;
        esac
    done
}



keyboard_menu
