#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../compat" && pwd -P)"
fixture="$(mktemp -d)"
trap 'rm -rf -- "$fixture"' EXIT

core="${1:?Compiled wallpaper core required}"

cat >"$fixture/forward.tsv" <<'EOF'
eDP-1	2560	1600	1.6	/fixtures/red.png	#cc2222	#ff2020
HDMI-A-1	1920	1080	1	/fixtures/blue.png	#2222cc	#2080ff
EOF
cat >"$fixture/reverse.tsv" <<'EOF'
HDMI-A-1	1920	1080	1	/fixtures/blue.png	#2222cc	#2080ff
eDP-1	2560	1600	1.6	/fixtures/red.png	#cc2222	#ff2020
EOF

"$core" palette-plan "$fixture/forward.tsv" >"$fixture/forward.out"
"$core" palette-plan "$fixture/reverse.tsv" >"$fixture/reverse.out"
cmp -s "$fixture/forward.out" "$fixture/reverse.out"
grep -Fxq 'output_count=2' "$fixture/forward.out"
grep -Fxq 'asset_count=2' "$fixture/forward.out"
grep -Fq $'output	HDMI-A-1	/fixtures/blue.png	2073600.000000' \
    "$fixture/forward.out"
grep -Fq $'output	eDP-1	/fixtures/red.png	1600000.000000' \
    "$fixture/forward.out"

cat >"$fixture/deduplicated.tsv" <<'EOF'
eDP-1	2560	1600	1.6	/fixtures/shared.png	#315789	#23b5d3
HDMI-A-1	1920	1080	1	/fixtures/shared.png	#315789	#23b5d3
EOF
"$core" palette-plan "$fixture/deduplicated.tsv" >"$fixture/deduplicated.out"
grep -Fxq 'output_count=2' "$fixture/deduplicated.out"
grep -Fxq 'asset_count=1' "$fixture/deduplicated.out"
grep -Fq $'asset	/fixtures/shared.png	3673600.000000' \
    "$fixture/deduplicated.out"

# The global accent is a vivid perceptual medoid, not the potentially muddy
# weighted background mean.
average="$(sed -n 's/^average=//p' "$fixture/forward.out")"
accent="$(sed -n 's/^accent=//p' "$fixture/forward.out")"
[[ -n "$average" && -n "$accent" && "$average" != "$accent" ]]

mkdir -p \
    "$fixture/home" \
    "$fixture/config/anto426-local/theme" \
    "$fixture/config/anto426-local/wallpaper/outputs" \
    "$fixture/cache/awww" \
    "$fixture/state"
magick -size 48x32 xc:'#d02070' "$fixture/boot.png"
printf '%s\n' '/fixtures/desktop-only.png' \
    >"$fixture/cache/awww/current-wallpaper.path"
printf '%s\n' 'desktop-palette-sentinel' \
    >"$fixture/config/anto426-local/wallpaper/desktop-palette.env"
printf '%s\n' 'desktop-theme-sentinel' \
    >"$fixture/config/anto426-local/theme/colors.css"
printf 'eDP-1\nimage\n/fixtures/desktop-only.png\n' \
    >"$fixture/config/anto426-local/wallpaper/outputs/desktop.state"

desktop_state_before="$(sha256sum \
    "$fixture/config/anto426-local/wallpaper/desktop-palette.env")"
desktop_theme_before="$(sha256sum \
    "$fixture/config/anto426-local/theme/colors.css")"
desktop_owner_before="$(sha256sum \
    "$fixture/cache/awww/current-wallpaper.path")"
desktop_outputs_before="$(sha256sum \
    "$fixture/config/anto426-local/wallpaper/outputs/desktop.state")"

HOME="$fixture/home" \
XDG_CONFIG_HOME="$fixture/config" \
XDG_CACHE_HOME="$fixture/cache" \
XDG_STATE_HOME="$fixture/state" \
ANTO_LOCAL_CONFIG_ROOT="$fixture/config/anto426-local" \
ANTO426_SCRIPT_DIR="$script_dir" \
    "$core" boot "$fixture/boot.png" >"$fixture/boot-generation.path"

[[ "$(sha256sum "$fixture/config/anto426-local/wallpaper/desktop-palette.env")" == \
   "$desktop_state_before" ]]
[[ "$(sha256sum "$fixture/config/anto426-local/theme/colors.css")" == \
   "$desktop_theme_before" ]]
[[ "$(sha256sum "$fixture/cache/awww/current-wallpaper.path")" == \
   "$desktop_owner_before" ]]
[[ "$(sha256sum "$fixture/config/anto426-local/wallpaper/outputs/desktop.state")" == \
   "$desktop_outputs_before" ]]

boot_state="$fixture/config/anto426-local/wallpaper/boot-login.env"
grep -Fxq 'domain=boot-login' "$boot_state"
grep -Fq "asset='$fixture/boot.png'" "$boot_state"
generation="$(sed -n "s/^generation_dir='\(.*\)'$/\1/p" "$boot_state")"
[[ -n "$generation" && -s "$generation/.complete" ]]
[[ -s "$generation/grub/background.jpg" ]]
[[ -s "$generation/grub/theme.txt" ]]
[[ -s "$generation/sddm/Backgrounds/anto426-current.png" ]]
[[ -s "$generation/sddm/theme.conf" ]]

# Desktop planning remains exactly the two output assets; the selected
# boot/login asset never participates in that aggregate.
"$core" palette-plan "$fixture/forward.tsv" >"$fixture/after-boot.out"
cmp -s "$fixture/forward.out" "$fixture/after-boot.out"
if grep -Fq "$fixture/boot.png" "$fixture/after-boot.out"; then
    printf 'boot/login asset leaked into desktop palette\n' >&2
    exit 1
fi

# Exercise the real effects collector against centralized per-output state.
# All consumers remain inside the fixture and both live reload workers are
# disabled; only the aggregate state/theme files are generated.
magick -size 64x48 xc:'#c92342' "$fixture/red.png"
magick -size 64x48 xc:'#245ac7' "$fixture/blue.png"
mkdir -p \
    "$fixture/no-scripts" \
    "$fixture/bin"
printf 'eDP-1\nimage\n%s\n' "$fixture/red.png" \
    >"$fixture/config/anto426-local/wallpaper/outputs/edp.state"
printf 'HDMI-A-1\nimage\n%s\n' "$fixture/blue.png" \
    >"$fixture/config/anto426-local/wallpaper/outputs/hdmi.state"
printf 'eDP-1\t2560\t1600\t1.6\nHDMI-A-1\t1920\t1080\t1\n' \
    >"$fixture/monitors.tsv"
printf '#!/usr/bin/env bash\nexit 0\n' >"$fixture/bin/notify-send"
chmod 755 "$fixture/bin/notify-send"
boot_state_before="$(sha256sum "$boot_state")"

HOME="$fixture/home" \
XDG_CONFIG_HOME="$fixture/config" \
XDG_CACHE_HOME="$fixture/cache" \
XDG_STATE_HOME="$fixture/state" \
ANTO_LOCAL_CONFIG_ROOT="$fixture/config/anto426-local" \
ANTO426_SCRIPT_DIR="$fixture/no-scripts" \
ANTO426_WALLPAPER_MONITORS_FILE="$fixture/monitors.tsv" \
ANTO426_WALLPAPER_CORE_MODULES=0 \
ANTO426_WALLPAPER_CORE_ICONS=0 \
PATH="$fixture/bin:/usr/bin:/bin" \
    "$core" effects "$fixture/red.png"

desktop_state="$fixture/config/anto426-local/wallpaper/desktop-palette.env"
grep -Fxq 'domain=desktop' "$desktop_state"
grep -Fxq 'output_count=2' "$desktop_state"
grep -Fxq 'asset_count=2' "$desktop_state"
grep -Fq "asset_0='" "$desktop_state"
grep -Fq 'asset_0_weight=' "$desktop_state"
[[ "$(sha256sum "$boot_state")" == "$boot_state_before" ]]

printf 'multi-wallpaper palette fixture: ok\n'
