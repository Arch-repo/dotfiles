#!/usr/bin/env bash
# Palette generation from wallpaper RGB samples.
# Outputs semantic tokens consumed by Hyprland, Rofi, GTK, Ghostty, GRUB, SDDM.

generate_palette_from_samples() {
    local r="$1"
    local g="$2"
    local b="$3"
    local ar="$4"
    local ag="$5"
    local ab="$6"

    read -r background surface select accent border foreground muted red orange yellow green pink purple gray base base_alt titlebar titlebar_backdrop popover selected_fg < <(
        awk -v r="$r" -v g="$g" -v b="$b" -v ar="$ar" -v ag="$ag" -v ab="$ab" '
            function clamp(v) { return v < 0 ? 0 : (v > 255 ? 255 : int(v + 0.5)) }
            function hex(rr, gg, bb) { return sprintf("#%02x%02x%02x", clamp(rr), clamp(gg), clamp(bb)) }
            function mix(a, b, ratio) { return a * (1 - ratio) + b * ratio }
            function brightness(rr, gg, bb) { return (299 * rr + 587 * gg + 114 * bb) / 1000 }
            function max3(a, b, c) { m = a > b ? a : b; return m > c ? m : c }
            function min3(a, b, c) { m = a < b ? a : b; return m < c ? m : c }
            function linear(v) {
                v = v / 255
                return v <= 0.03928 ? v / 12.92 : ((v + 0.055) / 1.055) ^ 2.4
            }
            function luminance(rr, gg, bb) {
                return 0.2126 * linear(rr) + 0.7152 * linear(gg) + 0.0722 * linear(bb)
            }
            function contrast(l1, l2,    tmp) {
                if (l1 < l2) {
                    tmp = l1
                    l1 = l2
                    l2 = tmp
                }
                return (l1 + 0.05) / (l2 + 0.05)
            }
            BEGIN {
                accent_chroma = max3(ar, ag, ab) - min3(ar, ag, ab)
                if (accent_chroma < 24) {
                    ar = clamp(r * 0.28 + 120 * 0.72)
                    ag = clamp(g * 0.28 + 168 * 0.72)
                    ab = clamp(b * 0.28 + 228 * 0.72)
                }

                accent_brightness = brightness(ar, ag, ab)
                if (accent_brightness < 92) {
                    ar = mix(ar, 255, 0.40)
                    ag = mix(ag, 255, 0.40)
                    ab = mix(ab, 255, 0.40)
                } else if (accent_brightness > 205) {
                    ar = mix(ar, 0, 0.18)
                    ag = mix(ag, 0, 0.18)
                    ab = mix(ab, 0, 0.18)
                } else {
                    ar = mix(ar, 255, 0.10)
                    ag = mix(ag, 255, 0.10)
                    ab = mix(ab, 255, 0.10)
                }

                bg_r = clamp(r * 0.26 + ar * 0.07 + 8)
                bg_g = clamp(g * 0.26 + ag * 0.07 + 8)
                bg_b = clamp(b * 0.26 + ab * 0.07 + 8)

                if (brightness(bg_r, bg_g, bg_b) > 112) {
                    bg_r = mix(bg_r, 0, 0.18)
                    bg_g = mix(bg_g, 0, 0.18)
                    bg_b = mix(bg_b, 0, 0.18)
                } else if (brightness(bg_r, bg_g, bg_b) < 20) {
                    bg_r = mix(bg_r, 255, 0.06)
                    bg_g = mix(bg_g, 255, 0.06)
                    bg_b = mix(bg_b, 255, 0.06)
                }

                accent_r = clamp(ar)
                accent_g = clamp(ag)
                accent_b = clamp(ab)

                surface_r = clamp(bg_r * 0.68 + r * 0.14 + accent_r * 0.08 + 255 * 0.10)
                surface_g = clamp(bg_g * 0.68 + g * 0.14 + accent_g * 0.08 + 255 * 0.10)
                surface_b = clamp(bg_b * 0.68 + b * 0.14 + accent_b * 0.08 + 255 * 0.10)

                select_r = clamp(bg_r * 0.48 + accent_r * 0.42 + 255 * 0.10)
                select_g = clamp(bg_g * 0.48 + accent_g * 0.42 + 255 * 0.10)
                select_b = clamp(bg_b * 0.48 + accent_b * 0.42 + 255 * 0.10)

                border_r = clamp(bg_r * 0.40 + accent_r * 0.38 + 255 * 0.22)
                border_g = clamp(bg_g * 0.40 + accent_g * 0.38 + 255 * 0.22)
                border_b = clamp(bg_b * 0.40 + accent_b * 0.38 + 255 * 0.22)

                bg_luma = luminance(bg_r, bg_g, bg_b)
                fg = contrast(bg_luma, luminance(246, 247, 251)) >= contrast(bg_luma, luminance(17, 17, 27)) ? "#f6f7fb" : "#11111b"
                if (fg == "#f6f7fb")
                    muted = hex(mix(bg_r, 246, 0.62), mix(bg_g, 247, 0.62), mix(bg_b, 251, 0.62))
                else
                    muted = hex(mix(bg_r, 17, 0.58), mix(bg_g, 17, 0.58), mix(bg_b, 27, 0.58))

                background = hex(bg_r, bg_g, bg_b)
                surface = hex(surface_r, surface_g, surface_b)
                select = hex(select_r, select_g, select_b)
                accent = hex(accent_r, accent_g, accent_b)
                border = hex(border_r, border_g, border_b)

                base = hex(mix(bg_r, surface_r, 0.24), mix(bg_g, surface_g, 0.24), mix(bg_b, surface_b, 0.24))
                base_alt = hex(mix(bg_r, surface_r, 0.62), mix(bg_g, surface_g, 0.62), mix(bg_b, surface_b, 0.62))
                titlebar = hex(mix(surface_r, accent_r, 0.08), mix(surface_g, accent_g, 0.08), mix(surface_b, accent_b, 0.08))
                titlebar_backdrop = hex(mix(bg_r, surface_r, 0.78), mix(bg_g, surface_g, 0.78), mix(bg_b, surface_b, 0.78))
                popover = hex(mix(bg_r, surface_r, 0.78), mix(bg_g, surface_g, 0.78), mix(bg_b, surface_b, 0.78))
                accent_luma = luminance(accent_r, accent_g, accent_b)
                selected_fg = contrast(accent_luma, luminance(17, 17, 27)) >= contrast(accent_luma, luminance(246, 247, 251)) ? "#11111b" : "#f6f7fb"

                red = hex(ar * 0.15 + 243 * 0.85, ag * 0.15 + 139 * 0.85, ab * 0.15 + 168 * 0.85)
                orange = hex(ar * 0.12 + 250 * 0.88, ag * 0.12 + 179 * 0.88, ab * 0.10 + 135 * 0.90)
                yellow = hex(ar * 0.08 + 249 * 0.92, ag * 0.08 + 226 * 0.92, ab * 0.05 + 175 * 0.95)
                green = hex(ar * 0.12 + 166 * 0.88, ag * 0.15 + 227 * 0.85, ab * 0.12 + 161 * 0.88)
                pink = hex(ar * 0.15 + 245 * 0.85, ag * 0.15 + 194 * 0.85, ab * 0.15 + 231 * 0.85)
                purple = hex(ar * 0.15 + 203 * 0.85, ag * 0.12 + 166 * 0.88, ab * 0.15 + 247 * 0.85)
                gray = hex(r * 0.20 + 108 * 0.80, g * 0.20 + 112 * 0.80, b * 0.20 + 134 * 0.80)

                print background, surface, select, accent, border, fg, muted, red, orange, yellow, green, pink, purple, gray, base, base_alt, titlebar, titlebar_backdrop, popover, selected_fg
            }
        '
    )

    panel_bg="$(hex_to_rofi_rgba "$background" 62)"
    panel_bg_hover="$(hex_to_rofi_rgba "$background" 78)"
    overlay_bg="$(hex_to_rofi_rgba "$background" 28)"
    item_bg="$(hex_to_rofi_rgba "$surface" 18)"
    item_bg_hover="$(hex_to_rofi_rgba "$select" 30)"
    item_bg_active="$(hex_to_rofi_rgba "$select" 42)"
    accent_soft="$(hex_to_rofi_rgba "$accent" 22)"
    accent_strong="$(hex_to_rofi_rgba "$accent" 42)"
    border_soft="$(hex_to_rofi_rgba "$border" 16)"
    border_medium="$(hex_to_rofi_rgba "$border" 34)"
    shadow_soft="$(hex_to_rofi_rgba "$background" 18)"
    shadow_medium="$(hex_to_rofi_rgba "$background" 30)"

    background_alpha="$panel_bg"
    surface_alpha="$item_bg"
    select_alpha="$item_bg_active"
    accent_alpha="$accent_strong"
    border_alpha="$border_medium"
}

write_palette_map() {
    local map_file="${1:-$state_dir/palette.map}"
    [[ -n "${state_dir:-}" ]] || return 0

    mkdir -p "$(dirname "$map_file")"
    cat >"$map_file" <<EOF
# Generated by wallpaper_effects.sh — palette routing map
# wallpaper=$current_wallpaper_path

[core]
background=$background
surface=$surface
base=$base
base_alt=$base_alt
foreground=$foreground
muted=$muted
accent=$accent
select=$select
border=$border
selected_fg=$selected_fg

[semantic]
titlebar=$titlebar
titlebar_backdrop=$titlebar_backdrop
popover=$popover
red=$red
orange=$orange
yellow=$yellow
green=$green
pink=$pink
purple=$purple
gray=$gray

[targets.session]
hypr=theme.generated.conf
colors=colors.{css,rasi,sh}
ghostty=dynamic.conf + themes/anto426
vscode=$vscode_theme_file
htop=$HOME/.config/htop/htoprc

[targets.apps]
gtk3=$gtk3_dir/gtk.css
gtk4=$gtk4_dir/gtk.css
qt5ct=$qt5ct_dir/colors/anto426.conf
qt6ct=$qt6ct_dir/colors/anto426.conf
kvantum=$kvantum_theme_dir

[targets.boot]
grub=$grub_theme_dir
sddm=$sddm_background

[htop.tokens]
color_scheme=0
source=terminal-ansi-palette
EOF
}
