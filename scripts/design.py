#!/usr/bin/env python3
"""Compile the same design tokens into C constants and GTK3/GTK4 styles."""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parent.parent

def generate(destination):
    tokens = json.loads((ROOT / "design/tokens.json").read_text())
    values = {f"{group}.{key}": value for group, entries in tokens.items()
              if group != "colour" for key, value in entries.items()}
    destination.mkdir(parents=True, exist_ok=True)
    header = ["#pragma once", "/* Generated from design/tokens.json. */"]
    header += [f"#define ANTO_{name.replace('.', '_').upper()} {value}" for name, value in values.items()]
    write(destination / "design_tokens.h", "\n".join(header) + "\n")
    def resolve(text):
        return re.sub(r"\{\{([a-z_.]+)\}\}", lambda match: str(values[match[1]]), text)
    colours = "\n".join(f"@define-color {name} {resolve(value)};" for name, value in tokens["colour"].items())
    write(destination / "design.css", colours + "\n")
    def render(source):
        return resolve(source.read_text())
    primitive_style = render(ROOT / "native/common/assets/primitives.css.in") + "\n" + render(ROOT / "native/common/assets/material.css.in")
    write(destination / "primitives.css", primitive_style)
    aliases = json.loads((ROOT / "design/surface-controls.json").read_text())
    def adapt_controls(surface):
        # Alias the same rule bodies to each surface's widget tree.
        clean = re.sub(r"/\*.*?\*/", "", primitive_style, flags=re.S)
        rules = []
        for match in re.finditer(r"([^{}]+)\{([^{}]*)\}", clean):
            selectors = []
            for selector in match[1].split(","):
                selectors.extend(aliases.get(surface, {}).get(selector.strip(), []))
            if selectors:
                rules.append(",\n".join(selectors) + " {" + match[2] + "}")
        return "\n/* Appearance adapted from the shared primitive stylesheet. */\n" + "\n".join(rules) + "\n"
    menu = ROOT / "native/menu/assets"
    # The entrypoint composes responsibilities in a fixed cascade order.
    sources = [menu / "components" / (part + ".css.in") for part in ("shell", "controls", "pages")]
    write(destination / "menu.css", "\n".join(render(source) for source in sources))
    write(destination / "widgets.css", render(ROOT / "native/widgets/assets/widgets.css.in"))
    for name in ("wallpaper", "osd", "glass"):
        write(destination / (name + ".css"), render(menu / (name + ".css.in")) +
              (adapt_controls("native") if name == "glass" else ""))
    for name in ("waybar", "swaync"):
        # GTK3 Waybar cannot consume CSS custom properties. Render numeric
        # tokens at build time; palette colours remain dynamic at runtime.
        write(destination / (name + ".css"),
              '@import url("../../../.config/anto426-local/theme/colors.css");\n' + colours + "\n" +
              render(ROOT / "native/shell/assets" / (name + ".css.in")) + adapt_controls(name))
    # Qt6 and Hyprlock consume the same numeric foundation as GTK.
    qml = ["import QtQuick", "QtObject {"]
    qml += [f"    readonly property real {name.replace('.', '_')}: {value}" for name, value in values.items()]
    write(destination / "Tokens.qml", "\n".join(qml) + "\n}\n")
    # Hyprlock/Pango takes points; Qt/GTK numeric font tokens use pixels.
    lock_source=(ROOT / "native/lock/hyprlock.conf.in").read_text()
    lock_source=re.sub(r"\{\{(font\.[a-z_]+)\}\}", lambda match: str(round(values[match[1]] * 72 / 96)), lock_source)
    write(destination / "hyprlock.conf", resolve(lock_source))
    palette = dict(re.findall(r"@define-color ([a-z-]+) (#[0-9a-fA-F]{6});", (ROOT / "native/menu/assets/tokens.css").read_text()))
    defaults = ["# Complete palette fallback, also used on first installation.", "$anto426_wallpaper ="]
    defaults += [f"$anto426_{name.replace('-', '_')} = rgb({value[1:]})" for name, value in palette.items()]
    defaults += [f"$anto426_{role} = rgb({palette[name][1:]})" for role, name in (("error", "red"), ("warning", "yellow"), ("success", "green"))]
    bg = palette["background"][1:]; fg = palette["foreground"][1:]
    defaults += [f"$anto426_panel_bg = rgba({bg}75)", f"$anto426_border_panel = rgba({fg}35)", f"$anto426_surface_soft = rgba({fg}11)", f"$anto426_active_border = rgba({palette['accent'][1:]}ee)", f"$anto426_inactive_border = rgba({bg}aa)"]
    write(destination / "hyprlock-defaults.conf", "\n".join(defaults) + "\n")
    write(destination / "terminal.conf", resolve(
        "# Generated from shared shell material tokens.\n"
        "background-opacity = {{material.opacity}}\n"
        "background-opacity-cells = true\n"
        "background-blur = true\n"
        "window-padding-x = {{spacing.lg}}\n"
        "window-padding-y = {{spacing.md}}\n"))
    write(destination / "terminal-hypr.conf", resolve(
        "# Keep text opaque; the application controls background alpha.\n"
        "windowrule = opacity 1.0 1.0 1.0, match:class ^(ghostty|com[.]mitchellh[.]ghostty)$\n"
        "windowrule = rounding {{radius.panel}}, match:class ^(ghostty|com[.]mitchellh[.]ghostty)$\n"))

def write(path, text):
    if not path.exists() or path.read_text() != text:
        path.write_text(text)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    generate(parser.parse_args().destination)
