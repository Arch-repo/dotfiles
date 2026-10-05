#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../compat" && pwd -P)"
fixture="$(mktemp -d)"
trap 'rm -rf -- "$fixture"' EXIT

local_root="$fixture/local/anto426-local"
generation="$local_root/wallpaper/boot-login/generations/0123456789abcdef"
grub="$fixture/system/grub"
sddm="$fixture/system/sddm"
mkdir -p "$generation/grub" "$generation/sddm/Backgrounds" \
    "$grub" "$sddm/Backgrounds"

printf 'grub-background\n' >"$generation/grub/background.jpg"
printf 'grub-theme\n' >"$generation/grub/theme.txt"
printf 'select-center\n' >"$generation/grub/select_c.png"
printf 'sddm-background\n' \
    >"$generation/sddm/Backgrounds/anto426-current.png"
printf 'sddm-theme\n' >"$generation/sddm/theme.conf"

ANTO_LOCAL_CONFIG_ROOT="$local_root" \
ANTO426_GRUB_THEME_DIR="$grub" \
ANTO426_SDDM_THEME_DIR="$sddm" \
    "$script_dir/wallpaper_boot_commit.sh" "$generation" >/dev/null

cmp "$generation/grub/background.jpg" "$grub/background.jpg"
cmp "$generation/grub/theme.txt" "$grub/theme.txt"
cmp "$generation/grub/select_c.png" "$grub/select_c.png"
cmp "$generation/sddm/Backgrounds/anto426-current.png" \
    "$sddm/Backgrounds/anto426-current.png"
cmp "$generation/sddm/theme.conf" "$sddm/theme.conf"
[[ ! -e "$grub/select_e.png" && ! -e "$grub/select_w.png" ]]

outside="$fixture/outside"
mkdir -p "$outside/grub" "$outside/sddm/Backgrounds"
printf 'x\n' >"$outside/grub/background.jpg"
printf 'x\n' >"$outside/grub/theme.txt"
printf 'x\n' >"$outside/sddm/Backgrounds/anto426-current.png"
printf 'x\n' >"$outside/sddm/theme.conf"
if ANTO_LOCAL_CONFIG_ROOT="$local_root" \
    ANTO426_GRUB_THEME_DIR="$grub" \
    ANTO426_SDDM_THEME_DIR="$sddm" \
    "$script_dir/wallpaper_boot_commit.sh" "$outside" 2>/dev/null; then
    printf 'commit ha accettato una generazione esterna\n' >&2
    exit 1
fi

printf 'algorithm=boot-login-v3\n' >"$generation/.complete"
if ANTO_LOCAL_CONFIG_ROOT="$local_root" ANTO426_GRUB_THEME_DIR="$grub" ANTO426_SDDM_THEME_DIR="$sddm" \
    "$script_dir/wallpaper_boot_commit.sh" "$generation" 2>/dev/null; then
    printf 'commit ha accettato una generazione v3 con selezione incompleta\n' >&2
    exit 1
fi
cmp "$generation/grub/background.jpg" "$grub/background.jpg"
printf 'boot/login commit fixture: ok\n'
