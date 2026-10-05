#!/usr/bin/env bash
set -euo pipefail

# Installa una generazione gia renderizzata nei due temi attivi.  Questo
# script non accetta percorsi arbitrari: la sorgente deve appartenere alla
# root centralizzata boot-login ed essere completa.
local_root="${ANTO_LOCAL_CONFIG_ROOT:-${XDG_CONFIG_HOME:-$HOME/.config}/anto426-local}"
generation_root="$local_root/wallpaper/boot-login/generations"
generation="${1:-}"
grub_theme_dir="${ANTO426_GRUB_THEME_DIR:-/usr/share/grub/themes/anto426}"
sddm_theme_dir="${ANTO426_SDDM_THEME_DIR:-/usr/share/sddm/themes/anto426-sddm}"

if [[ -z "$generation" ]]; then
    printf 'Uso: %s /generazione/boot-login\n' "$0" >&2
    exit 2
fi

generation_root="$(readlink -f -- "$generation_root" 2>/dev/null || true)"
generation="$(readlink -f -- "$generation" 2>/dev/null || true)"
if [[ -z "$generation_root" || -z "$generation" || ! -d "$generation" ]]; then
    printf 'Generazione Avvio e login non valida\n' >&2
    exit 1
fi
case "$generation" in
    "$generation_root"/*) ;;
    *)
        printf 'Generazione fuori dalla root autorizzata: %s\n' "$generation" >&2
        exit 1
        ;;
esac

sources=(
    "$generation/grub/background.jpg"
    "$generation/grub/theme.txt"
    "$generation/sddm/Backgrounds/anto426-current.png"
    "$generation/sddm/theme.conf"
)
destinations=(
    "$grub_theme_dir/background.jpg"
    "$grub_theme_dir/theme.txt"
    "$sddm_theme_dir/Backgrounds/anto426-current.png"
    "$sddm_theme_dir/theme.conf"
)

for source in "${sources[@]}"; do
    if [[ ! -f "$source" || -L "$source" || ! -s "$source" ]]; then
        printf 'Generazione incompleta o non sicura: %s\n' "$source" >&2
        exit 1
    fi
done

require_nine_slices=0
if [[ -f "$generation/.complete" ]] && grep -q '^algorithm=boot-login-v3$' "$generation/.complete"; then
    require_nine_slices=1
fi
for optional in select_c.png select_n.png select_ne.png select_e.png select_se.png select_s.png select_sw.png select_w.png select_nw.png; do
    source="$generation/grub/$optional"
    if [[ -f "$source" && ! -L "$source" && -s "$source" ]]; then
        sources+=("$source")
        destinations+=("$grub_theme_dir/$optional")
    elif ((require_nine_slices)); then
        printf 'Generazione incompleta: manca %s\n' "$optional" >&2
        exit 1
    fi
done

temporary_files=()
cleanup() {
    local temporary
    for temporary in "${temporary_files[@]}"; do
        [[ -n "$temporary" ]] && rm -f -- "$temporary"
    done
    return 0
}
trap cleanup EXIT

# Tutti i target devono essere scrivibili prima del primo commit: niente
# installazioni parziali e nessun prompt privilegiato nel mezzo.
for destination in "${destinations[@]}"; do
    directory="$(dirname -- "$destination")"
    if [[ ! -d "$directory" || ! -w "$directory" ]]; then
        printf 'Tema di sistema non scrivibile: %s\n' "$directory" >&2
        printf 'Esegui una volta il setup amministrativo dei dotfile, poi riprova.\n' >&2
        exit 1
    fi
done

for index in "${!sources[@]}"; do
    destination="${destinations[$index]}"
    temporary="$destination.anto426-new.$$"
    install -m 0644 -- "${sources[$index]}" "$temporary"
    temporary_files+=("$temporary")
done

for index in "${!destinations[@]}"; do
    destination="${destinations[$index]}"
    temporary="${temporary_files[$index]}"
    mv -fT -- "$temporary" "$destination"
    temporary_files[$index]=""
done

sync -f "$grub_theme_dir" 2>/dev/null || true
sync -f "$sddm_theme_dir" 2>/dev/null || true
printf 'Avvio e login aggiornati dalla generazione %s\n' "$(basename -- "$generation")"
