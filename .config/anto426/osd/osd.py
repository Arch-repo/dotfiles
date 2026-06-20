#!/usr/bin/env python3
"""Centred GTK OSD slider for volume, mic and brightness."""

from __future__ import annotations

import os
import signal
import sys
from pathlib import Path

import gi

gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
try:
    gi.require_version("GtkLayerShell", "0.1")
    from gi.repository import GtkLayerShell  # noqa: E402

    HAS_LAYER_SHELL = True
except (ImportError, ValueError):
    GtkLayerShell = None
    HAS_LAYER_SHELL = False
from gi.repository import Gdk, GLib, Gtk  # noqa: E402

STATE_FILE = Path(
    os.environ.get(
        "ANTO426_OSD_STATE",
        f"{os.environ.get('XDG_RUNTIME_DIR', '/tmp')}/anto426-osd.state",
    )
)
PID_FILE = Path(
    os.environ.get(
        "ANTO426_OSD_PID",
        f"{os.environ.get('XDG_RUNTIME_DIR', '/tmp')}/anto426-osd.pid",
    )
)
CSS_FILE = Path(__file__).with_name("style.css")
COLORS_FILE = Path(
    os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config"))
) / "colors" / "colors.css"
HIDE_MS = 1400
OSD_WIDTH = 340
OSD_HEIGHT = 84
BAR_WIDTH = 280
BOTTOM_MARGIN = 48

FALLBACK_CSS = b"""
@define-color background #1e1e2e;
@define-color surface #313244;
@define-color foreground #f6f7fb;
@define-color muted #b9c4d2;
@define-color accent #8cb8e4;
@define-color border #6c7086;
@define-color panel-bg rgba(30, 30, 46, 0.62);
@define-color overlay-bg rgba(30, 30, 46, 0.28);
@define-color item-bg rgba(49, 50, 68, 0.18);
@define-color item-bg-active rgba(140, 184, 228, 0.42);
@define-color border-medium rgba(108, 112, 134, 0.34);
@define-color background-alpha @panel-bg;
@define-color surface-alpha @item-bg;
"""

ICONS = {
    "volume": "󰕾",
    "brightness": "󰃠",
    "mic": "󰍬",
}

TITLES = {
    "volume": "Volume",
    "brightness": "Luminosità",
    "mic": "Microfono",
}


class OsdWindow(Gtk.Window):
    def __init__(self) -> None:
        super().__init__(type=Gtk.WindowType.TOPLEVEL)
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_keep_above(True)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_accept_focus(False)
        self.set_type_hint(Gdk.WindowTypeHint.NOTIFICATION)
        self.set_app_paintable(True)
        self.get_style_context().add_class("osd-root")
        self.set_name("anto426-osd")
        try:
            self.set_wmclass("anto426-osd", "anto426-osd")
        except AttributeError:
            pass
        self._configure_surface()

        self._load_css()

        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        outer.get_style_context().add_class("osd-box")
        self.add(outer)

        header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        outer.pack_start(header, False, False, 0)

        self.icon_label = Gtk.Label()
        self.icon_label.get_style_context().add_class("osd-icon")
        header.pack_start(self.icon_label, False, False, 0)

        text_col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        header.pack_start(text_col, True, True, 0)

        self.title_label = Gtk.Label(xalign=0)
        self.title_label.get_style_context().add_class("osd-title")
        text_col.pack_start(self.title_label, False, False, 0)

        self.value_label = Gtk.Label(xalign=0)
        self.value_label.get_style_context().add_class("osd-value")
        text_col.pack_start(self.value_label, False, False, 0)

        self.progress = Gtk.ProgressBar()
        self.progress.set_show_text(False)
        self.progress.set_size_request(BAR_WIDTH, 8)
        self.progress.get_style_context().add_class("osd-track")
        outer.pack_start(self.progress, False, False, 0)

        self._hide_id: int | None = None
        self.show_all()
        self._centre()

    def _load_css(self) -> None:
        screen = Gdk.Screen.get_default()
        if screen is None:
            return

        fallback = Gtk.CssProvider()
        fallback.load_from_data(FALLBACK_CSS)
        Gtk.StyleContext.add_provider_for_screen(
            screen,
            fallback,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )

        if COLORS_FILE.exists():
            colors = Gtk.CssProvider()
            colors.load_from_path(str(COLORS_FILE))
            Gtk.StyleContext.add_provider_for_screen(
                screen,
                colors,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION + 1,
            )

        provider = Gtk.CssProvider()
        provider.load_from_path(str(CSS_FILE))
        Gtk.StyleContext.add_provider_for_screen(
            screen,
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION + 2,
        )

    def _configure_surface(self) -> None:
        screen = self.get_screen()
        visual = screen.get_rgba_visual() if screen is not None else None
        if visual is not None:
            self.set_visual(visual)

        if HAS_LAYER_SHELL and GtkLayerShell is not None:
            GtkLayerShell.init_for_window(self)
            GtkLayerShell.set_namespace(self, "anto426-osd")
            GtkLayerShell.set_layer(self, GtkLayerShell.Layer.OVERLAY)
            GtkLayerShell.set_keyboard_mode(self, GtkLayerShell.KeyboardMode.NONE)
            GtkLayerShell.set_exclusive_zone(self, -1)
            GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.LEFT, True)
            GtkLayerShell.set_anchor(self, GtkLayerShell.Edge.BOTTOM, True)
            return

        self.set_position(Gtk.WindowPosition.CENTER)

    def _centre(self) -> None:
        display = Gdk.Display.get_default()
        monitor = display.get_primary_monitor() if display else None
        if monitor is None and display is not None:
            monitor = display.get_monitor(0)
        if monitor is None:
            return
        geo = monitor.get_geometry()
        x = max(0, (geo.width - OSD_WIDTH) // 2)
        if HAS_LAYER_SHELL and GtkLayerShell is not None:
            GtkLayerShell.set_margin(self, GtkLayerShell.Edge.LEFT, x)
            GtkLayerShell.set_margin(self, GtkLayerShell.Edge.BOTTOM, BOTTOM_MARGIN)
            return

        y = max(0, geo.height - OSD_HEIGHT - BOTTOM_MARGIN)
        window = self.get_window()
        if window is not None:
            window.move(geo.x + x, geo.y + y)

    def _schedule_hide(self) -> None:
        if self._hide_id is not None:
            GLib.source_remove(self._hide_id)
        self._hide_id = GLib.timeout_add(HIDE_MS, self._hide)

    def _hide(self) -> bool:
        Gtk.main_quit()
        return False

    def update(self, kind: str, value: int, muted: bool = False) -> None:
        kind = kind if kind in ICONS else "volume"
        value = max(0, min(100, int(value)))
        muted = bool(muted)

        self.icon_label.set_text("󰝟" if muted else ICONS[kind])
        self.title_label.set_text(TITLES[kind])
        self.value_label.set_text("Muto" if muted else f"{value}%")

        self.progress.set_fraction(0.0 if muted else value / 100)
        if muted:
            self.progress.get_style_context().add_class("muted")
        else:
            self.progress.get_style_context().remove_class("muted")

        if not self.get_visible():
            self.show_all()
        self._centre()
        self._schedule_hide()


def read_state() -> tuple[str, int, bool]:
    try:
        raw = STATE_FILE.read_text(encoding="utf-8").strip().split()
    except OSError:
        raw = []
    if len(raw) < 2:
        return "volume", 0, False
    kind = raw[0]
    try:
        value = int(float(raw[1]))
    except ValueError:
        value = 0
    muted = len(raw) > 2 and raw[2] in {"1", "true", "yes", "muted"}
    return kind, value, muted


def write_state(kind: str, value: int, muted: bool = False) -> None:
    STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
    STATE_FILE.write_text(
        f"{kind} {value} {1 if muted else 0}\n",
        encoding="utf-8",
    )


def cleanup_pid_file() -> None:
    try:
        current = PID_FILE.read_text(encoding="utf-8").strip()
    except OSError:
        return
    if current == str(os.getpid()):
        try:
            PID_FILE.unlink()
        except OSError:
            pass


def main() -> int:
    if len(sys.argv) >= 2 and sys.argv[1] != "daemon":
        kind = sys.argv[1]
        value = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        muted = len(sys.argv) > 3 and sys.argv[3] in {"1", "true", "muted"}
        write_state(kind, value, muted)

    kind, value, muted = read_state()
    win = OsdWindow()
    win.update(kind, value, muted)

    def refresh_from_state() -> bool:
        k, v, m = read_state()
        win.update(k, v, m)
        return False

    def on_usr1(*_args: object) -> None:
        GLib.idle_add(refresh_from_state)

    signal.signal(signal.SIGUSR1, on_usr1)
    try:
        Gtk.main()
        return 0
    finally:
        cleanup_pid_file()


if __name__ == "__main__":
    raise SystemExit(main())
