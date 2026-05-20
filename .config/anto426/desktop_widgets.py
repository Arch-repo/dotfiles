#!/usr/bin/env python3
import math
import os
import random
import re
import subprocess
import sys
import time

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")

try:
    gi.require_version("GtkLayerShell", "0.1")
    from gi.repository import GtkLayerShell

    HAS_LAYER_SHELL = True
except (ImportError, ValueError):
    GtkLayerShell = None
    HAS_LAYER_SHELL = False

from gi.repository import Gdk, GLib, Gtk


WIDGETS = os.environ.get("ANTO426_WIDGETS_ENABLED", "clock cava system").split()
COLORS_FILE = os.path.expanduser("~/.config/colors/colors.sh")


def read_colors():
    colors = {
        "background": "#15151c",
        "surface": "#242634",
        "foreground": "#f4f4f5",
        "muted": "#a8adbd",
        "accent": "#89b4fa",
        "border": "#45475a",
    }

    if not os.path.exists(COLORS_FILE):
        return colors

    pattern = re.compile(r'^export ANTO426_([A-Z_]+)="?(#[0-9a-fA-F]{6})"?$')
    with open(COLORS_FILE, "r", encoding="utf-8", errors="ignore") as handle:
        for line in handle:
            match = pattern.match(line.strip())
            if not match:
                continue
            key = match.group(1).lower()
            if key in colors:
                colors[key] = match.group(2)
    return colors


def hex_to_rgba(color, alpha):
    color = color.lstrip("#")
    r = int(color[0:2], 16)
    g = int(color[2:4], 16)
    b = int(color[4:6], 16)
    return f"rgba({r}, {g}, {b}, {alpha:.2f})"


COLORS = read_colors()


def run_text(command):
    try:
        output = subprocess.check_output(
            command,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=0.8,
        )
    except (subprocess.SubprocessError, FileNotFoundError):
        return ""
    return output.strip()


def volume_percent():
    output = run_text(["wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@"])
    match = re.search(r"Volume:\s+([0-9.]+)", output)
    if not match:
        return 0
    return max(0, min(100, round(float(match.group(1)) * 100)))


def brightness_percent():
    output = run_text(["brightnessctl", "-m"])
    parts = output.split(",")
    if len(parts) < 4:
        return None
    try:
        return int(parts[3].strip().rstrip("%"))
    except ValueError:
        return None


def battery_percent():
    base = "/sys/class/power_supply"
    if not os.path.isdir(base):
        return None
    for name in os.listdir(base):
        path = os.path.join(base, name)
        try:
            with open(os.path.join(path, "type"), "r", encoding="utf-8") as handle:
                if handle.read().strip() != "Battery":
                    continue
            with open(os.path.join(path, "capacity"), "r", encoding="utf-8") as handle:
                return int(handle.read().strip())
        except (OSError, ValueError):
            continue
    return None


def bar(value, width=10):
    value = max(0, min(100, int(value)))
    filled = round(value / 100 * width)
    return "━" * filled + "─" * (width - filled)


class LayerWidget(Gtk.Window):
    def __init__(self, namespace, width, height, left, top):
        super().__init__(title=namespace)
        self.set_default_size(width, height)
        self.set_size_request(width, height)
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_accept_focus(False)
        self.set_focus_on_map(False)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_app_paintable(True)
        self.set_type_hint(Gdk.WindowTypeHint.DOCK)

        screen = self.get_screen()
        visual = screen.get_rgba_visual()
        if visual:
            self.set_visual(visual)

        GtkLayerShell.init_for_window(self)
        GtkLayerShell.set_namespace(self, namespace)
        GtkLayerShell.set_layer(self, GtkLayerShell.Layer.BOTTOM)
        GtkLayerShell.set_keyboard_mode(self, GtkLayerShell.KeyboardMode.NONE)
        GtkLayerShell.set_exclusive_zone(self, -1)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.TOP, True)
        GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.LEFT, True)
        GtkLayerShell.set_margin(self, GtkLayerShell.Edge.TOP, top)
        GtkLayerShell.set_margin(self, GtkLayerShell.Edge.LEFT, left)

        self.container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=6)
        self.container.set_name("WidgetCard")
        self.container.set_margin_start(18)
        self.container.set_margin_end(18)
        self.container.set_margin_top(14)
        self.container.set_margin_bottom(14)
        self.add(self.container)


class ClockWidget(LayerWidget):
    def __init__(self):
        super().__init__("anto426-clock-widget", 360, 170, 40, 86)
        self.time_label = Gtk.Label()
        self.time_label.set_name("ClockTime")
        self.time_label.set_xalign(0.0)
        self.date_label = Gtk.Label()
        self.date_label.set_name("MutedText")
        self.date_label.set_xalign(0.0)
        self.year_label = Gtk.Label()
        self.year_label.set_name("SmallText")
        self.year_label.set_xalign(0.0)
        self.container.pack_start(self.time_label, False, False, 0)
        self.container.pack_start(self.date_label, False, False, 0)
        self.container.pack_start(self.year_label, False, False, 0)
        self.update()
        GLib.timeout_add_seconds(1, self.update)

    def update(self):
        now = time.localtime()
        self.time_label.set_text(time.strftime("%H:%M", now))
        self.date_label.set_text(time.strftime("%A %d %B", now))
        self.year_label.set_text(time.strftime("%Y", now))
        return True


class CavaWidget(LayerWidget):
    def __init__(self):
        super().__init__("anto426-cava-widget", 520, 180, 40, 286)
        self.values = [0.15 for _ in range(28)]
        self.phase = 0.0
        self.title = Gtk.Label(label="Audio")
        self.title.set_name("WidgetTitle")
        self.title.set_xalign(0.0)
        self.area = Gtk.DrawingArea()
        self.area.set_size_request(480, 100)
        self.area.connect("draw", self.draw_bars)
        self.container.pack_start(self.title, False, False, 0)
        self.container.pack_start(self.area, True, True, 0)
        GLib.timeout_add(70, self.update)

    def update(self):
        volume = max(0.18, volume_percent() / 100)
        self.phase += 0.24
        for index, current in enumerate(self.values):
            wave = (math.sin(self.phase + index * 0.55) + 1) / 2
            target = (0.18 + wave * random.uniform(0.35, 0.95)) * volume
            self.values[index] = current * 0.62 + target * 0.38
        self.area.queue_draw()
        return True

    def draw_bars(self, _area, ctx):
        width = self.area.get_allocated_width()
        height = self.area.get_allocated_height()
        gap = 5
        bar_width = max(4, (width - gap * (len(self.values) - 1)) / len(self.values))
        accent = Gdk.RGBA()
        surface = Gdk.RGBA()
        accent.parse(COLORS["accent"])
        surface.parse(COLORS["surface"])

        for index, value in enumerate(self.values):
            x = index * (bar_width + gap)
            bar_height = max(10, height * min(1, value))
            y = height - bar_height
            ctx.set_source_rgba(surface.red, surface.green, surface.blue, 0.45)
            ctx.rectangle(x, 0, bar_width, height)
            ctx.fill()
            ctx.set_source_rgba(accent.red, accent.green, accent.blue, 0.88)
            ctx.rectangle(x, y, bar_width, bar_height)
            ctx.fill()
        return False


class SystemWidget(LayerWidget):
    def __init__(self):
        super().__init__("anto426-system-widget", 390, 230, 40, 490)
        self.title = Gtk.Label(label="Sistema")
        self.title.set_name("WidgetTitle")
        self.title.set_xalign(0.0)
        self.info = Gtk.Label()
        self.info.set_name("InfoText")
        self.info.set_xalign(0.0)
        self.info.set_yalign(0.0)
        self.container.pack_start(self.title, False, False, 0)
        self.container.pack_start(self.info, True, True, 0)
        self.update()
        GLib.timeout_add_seconds(5, self.update)

    def update(self):
        battery = battery_percent()
        brightness = brightness_percent()
        volume = volume_percent()
        mem = run_text(["bash", "-lc", "free -h | awk '/^Mem:/ {print $3 \" / \" $2}'"])
        load = run_text(["bash", "-lc", "cut -d' ' -f1-3 /proc/loadavg"])
        rows = [
            f"BAT  {battery if battery is not None else 'n/d'}%",
            f"RAM  {mem or 'n/d'}",
            f"CPU  {load or 'n/d'}",
            f"VOL  {volume}%  {bar(volume)}",
        ]
        if brightness is not None:
            rows.append(f"LUX  {brightness}%  {bar(brightness)}")
        else:
            rows.append("LUX  n/d")
        self.info.set_text("\n".join(rows))
        return True


def load_css():
    css = f"""
    window {{
        background-color: transparent;
    }}
    #WidgetCard {{
        background-color: {hex_to_rgba(COLORS["background"], 0.58)};
        border: 1px solid {hex_to_rgba(COLORS["border"], 0.62)};
        border-radius: 14px;
        color: {COLORS["foreground"]};
    }}
    #ClockTime {{
        color: {COLORS["foreground"]};
        font-family: "JetBrainsMono Nerd Font";
        font-size: 42px;
        font-weight: 500;
    }}
    #WidgetTitle {{
        color: {COLORS["accent"]};
        font-family: "JetBrainsMono Nerd Font";
        font-size: 14px;
        font-weight: 700;
    }}
    #MutedText {{
        color: {COLORS["muted"]};
        font-family: "JetBrainsMono Nerd Font";
        font-size: 13px;
        font-weight: 500;
    }}
    #SmallText {{
        color: {COLORS["muted"]};
        font-family: "JetBrainsMono Nerd Font";
        font-size: 12px;
        font-weight: 500;
    }}
    #InfoText {{
        color: {COLORS["foreground"]};
        font-family: "JetBrainsMono Nerd Font";
        font-size: 13px;
        font-weight: 500;
    }}
    """
    provider = Gtk.CssProvider()
    provider.load_from_data(css.encode("utf-8"))
    Gtk.StyleContext.add_provider_for_screen(
        Gdk.Screen.get_default(),
        provider,
        Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
    )


def main():
    if "--check" in sys.argv:
        return 0 if HAS_LAYER_SHELL else 1

    if not HAS_LAYER_SHELL:
        print("GtkLayerShell non disponibile: i widget layer non possono partire.", file=sys.stderr)
        return 2

    load_css()
    windows = []
    if "clock" in WIDGETS:
        windows.append(ClockWidget())
    if "cava" in WIDGETS:
        windows.append(CavaWidget())
    if "system" in WIDGETS:
        windows.append(SystemWidget())

    if not windows:
        return 0

    for window in windows:
        window.connect("destroy", Gtk.main_quit)
        window.show_all()

    Gtk.main()
    return 0


if __name__ == "__main__":
    sys.exit(main())
