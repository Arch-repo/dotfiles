#!/usr/bin/env python3
"""Centred GTK OSD slider (CSS) for volume and brightness."""

from __future__ import annotations

import os
import signal
import sys
from pathlib import Path

import gi

gi.require_version("Gtk", "3.0")
from gi.repository import Gdk, GLib, Gtk  # noqa: E402

STATE_FILE = Path(
    os.environ.get(
        "ANTO426_OSD_STATE",
        f"{os.environ.get('XDG_RUNTIME_DIR', '/tmp')}/anto426-osd.state",
    )
)
CSS_FILE = Path(__file__).with_name("style.css")
HIDE_MS = 1400

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
        self.get_style_context().add_class("osd-root")
        self.set_name("anto426-osd")
        try:
            self.set_wmclass("anto426-osd", "anto426-osd")
        except AttributeError:
            pass

        provider = Gtk.CssProvider()
        provider.load_from_path(str(CSS_FILE))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION,
        )

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

        self.track = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self.track.get_style_context().add_class("osd-track")
        self.track.set_size_request(316, 10)
        outer.pack_start(self.track, False, False, 0)

        self.fill = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        self.fill.get_style_context().add_class("osd-fill")
        self.track.pack_start(self.fill, False, False, 0)

        self._hide_id: int | None = None
        self.show_all()
        self._centre()

    def _centre(self) -> None:
        display = Gdk.Display.get_default()
        monitor = display.get_primary_monitor() if display else None
        if monitor is None and display is not None:
            monitor = display.get_monitor(0)
        if monitor is None:
            return
        geo = monitor.get_geometry()
        self.get_window().move(
            geo.x + max(0, (geo.width - 400) // 2),
            geo.y + max(0, (geo.height - 120) // 2),
        )

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

        fill_width = 0 if muted else int(316 * value / 100)
        self.fill.set_size_request(max(0, fill_width), 10)
        if muted:
            self.fill.get_style_context().add_class("muted")
        else:
            self.fill.get_style_context().remove_class("muted")

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


def main() -> int:
    if len(sys.argv) >= 2 and sys.argv[1] != "daemon":
        kind = sys.argv[1]
        value = int(sys.argv[2]) if len(sys.argv) > 2 else 0
        muted = len(sys.argv) > 3 and sys.argv[3] in {"1", "true", "muted"}
        write_state(kind, value, muted)

    kind, value, muted = read_state()
    win = OsdWindow()
    win.update(kind, value, muted)

    def on_usr1(*_args: object) -> None:
        k, v, m = read_state()
        win.update(k, v, m)

    signal.signal(signal.SIGUSR1, on_usr1)
    Gtk.main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
