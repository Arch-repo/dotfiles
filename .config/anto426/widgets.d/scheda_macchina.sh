#!/usr/bin/env bash
set -uo pipefail

interval="${ANTO426_WIDGET_FASTFETCH_INTERVAL:-30}"
logo_mode="${ANTO426_WIDGET_FASTFETCH_LOGO:-image}"
logo_width="${ANTO426_WIDGET_FASTFETCH_LOGO_WIDTH:-18}"
logo_height="${ANTO426_WIDGET_FASTFETCH_LOGO_HEIGHT:-}"
logo_image="${ANTO426_WIDGET_FASTFETCH_IMAGE:-}"

printf '\033[?25l\033[?7l'
trap 'printf "\033[?7h\033[?25h"' EXIT

term_cols() {
    local cols
    cols="$(tput cols 2>/dev/null || true)"
    [[ "$cols" =~ ^[0-9]+$ ]] || cols=72
    printf '%s' "$cols"
}

term_lines() {
    local lines
    lines="$(tput lines 2>/dev/null || true)"
    [[ "$lines" =~ ^[0-9]+$ ]] || lines=18
    printf '%s' "$lines"
}

pick_logo_image() {
    find "$HOME/Pictures/neofetch" -type f \
        \( -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.png" -o -iname "*.webp" \) \
        2>/dev/null | shuf -n 1
}

clear_terminal() {
    printf '\033_Ga=d,d=A\033\\'
    printf '\033[2J\033[3J\033[H'
}

run_fastfetch() {
    local cols lines
    local -a logo_args common_args

    cols="$(term_cols)"
    lines="$(term_lines)"
    logo_args=(--logo none)

    case "$logo_mode" in
        small)
            logo_args=(--logo small)
            ;;
        image)
            if (( cols >= 52 && lines >= 14 )); then
                if [[ -z "$logo_image" ]]; then
                    logo_image="$(pick_logo_image)"
                fi
                if [[ -n "$logo_image" ]]; then
                    logo_args=(
                        --logo "$logo_image"
                        --logo-type kitty
                        --logo-width "$logo_width"
                        --logo-padding-left 1
                        --logo-padding-right 2
                        --logo-preserve-aspect-ratio true
                    )
                    if [[ -n "$logo_height" ]]; then
                        logo_args+=(--logo-height "$logo_height")
                    fi
                else
                    logo_args=(--logo small)
                fi
            else
                logo_args=(--logo small)
            fi
            ;;
    esac

    common_args=(
        --disable-linewrap true
        --hide-cursor true
        --key-width 8
        --separator ": "
        --structure Title:Separator:OS:Kernel:WM:CPU:Memory:Battery
    )

    fastfetch "${logo_args[@]}" "${common_args[@]}"
}

while true; do
    clear_terminal
    if command -v fastfetch >/dev/null 2>&1; then
        run_fastfetch
    else
        printf '\n  fastfetch non installato\n'
    fi
    sleep "$interval"
done
