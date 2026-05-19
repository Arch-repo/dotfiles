#!/usr/bin/env python3
import os
import sys
import subprocess
import re
import fcntl
import cairo
import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gdk', '3.0')
try:
    gi.require_version('GtkLayerShell', '0.1')
    from gi.repository import GtkLayerShell
    HAS_LAYER_SHELL = True
except (ImportError, ValueError):
    GtkLayerShell = None
    HAS_LAYER_SHELL = False
from gi.repository import Gtk, Gdk, GLib

WINDOW_TITLE = "Control Menu - Audio"
LAYER_NAMESPACE = "anto426-audio"
PANEL_WIDTH = 380
DISPLAY_NAME_MAX = 32
LOCK_FILE = os.path.join(
    os.environ.get("XDG_RUNTIME_DIR") or "/tmp",
    "anto426-audio-control.lock",
)


def focus_existing_window():
    try:
        subprocess.run(
            ["hyprctl", "dispatch", "focuswindow", f"title:^({WINDOW_TITLE})$"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except FileNotFoundError:
        pass


def acquire_single_instance_lock():
    os.makedirs(os.path.dirname(LOCK_FILE), exist_ok=True)
    lock_fd = open(LOCK_FILE, "a+")
    try:
        fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        focus_existing_window()
        lock_fd.close()
        return None

    lock_fd.seek(0)
    lock_fd.truncate()
    lock_fd.write(str(os.getpid()))
    lock_fd.flush()
    return lock_fd

class AudioControlCenter(Gtk.Window):
    def __init__(self):
        super().__init__(title=WINDOW_TITLE)
        self.set_default_size(PANEL_WIDTH, 1)
        self.set_size_request(PANEL_WIDTH, -1)
        self.set_decorated(False)
        self.set_resizable(False)
        self.set_app_paintable(True)
        self.set_accept_focus(True)
        self.set_skip_taskbar_hint(True)
        self.set_skip_pager_hint(True)
        self.set_type_hint(Gdk.WindowTypeHint.DIALOG)
        self.configure_surface()

        # Allow transparent background
        screen = self.get_screen()
        visual = screen.get_rgba_visual()
        if visual:
            self.set_visual(visual)

        # State flags to avoid feedback loops while dragging sliders
        self.updating_sinks = False
        self.updating_sources = False
        self.is_dragging_sink = False
        self.is_dragging_source = False
        self.can_close_on_focus_loss = False

        self.build_ui()
        self.load_styles()
        self.update_state()

        # Connect window events
        self.connect("draw", self.on_draw)
        self.connect("key-press-event", self.on_key_press)
        self.connect("destroy", Gtk.main_quit)
        self.connect("focus-out-event", self.on_focus_out)

        # Enable focus close after a short delay to survive Wayland startup mapping
        GLib.timeout_add(500, self.enable_focus_close)

        # Start background poll loop to sync slider when system volume changes
        GLib.timeout_add(250, self.update_state)

    def configure_surface(self):
        if HAS_LAYER_SHELL:
            GtkLayerShell.init_for_window(self)
            GtkLayerShell.set_namespace(self, LAYER_NAMESPACE)
            GtkLayerShell.set_layer(self, GtkLayerShell.Layer.OVERLAY)
            GtkLayerShell.set_keyboard_mode(self, GtkLayerShell.KeyboardMode.EXCLUSIVE)
            for edge in (
                GtkLayerShell.Edge.TOP,
                GtkLayerShell.Edge.RIGHT,
                GtkLayerShell.Edge.BOTTOM,
                GtkLayerShell.Edge.LEFT,
            ):
                GtkLayerShell.set_anchor(self, edge, False)
            return

        self.set_position(Gtk.WindowPosition.CENTER)
        self.set_keep_above(True)

    def build_ui(self):
        self.main_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=14)
        self.main_box.set_name("MainContainer")
        self.main_box.set_margin_start(20)
        self.main_box.set_margin_end(20)
        self.main_box.set_margin_top(20)
        self.main_box.set_margin_bottom(20)
        self.add(self.main_box)

        header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        header_box.get_style_context().add_class("inputbar")

        title_label = Gtk.Label()
        title_label.set_markup("<b>󰓃</b>")
        title_label.set_name("HeaderTitle")
        header_box.pack_start(title_label, False, False, 0)

        prompt_label = Gtk.Label(label="Audio")
        prompt_label.set_xalign(0.0)
        prompt_label.get_style_context().add_class("prompt")
        header_box.pack_start(prompt_label, True, True, 0)
        self.main_box.pack_start(header_box, False, False, 0)

        self.sink_card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.sink_card.get_style_context().add_class("section")
        self.main_box.pack_start(self.sink_card, False, False, 0)

        sink_header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        self.sink_card.pack_start(sink_header, False, False, 0)

        sink_icon = Gtk.Label(label="󰕾")
        sink_icon.get_style_context().add_class("card-icon")
        sink_header.pack_start(sink_icon, False, False, 0)

        sink_text_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        sink_header.pack_start(sink_text_box, True, True, 0)

        sink_title = Gtk.Label()
        sink_title.set_markup("<b>Uscita audio</b>")
        sink_title.set_xalign(0.0)
        sink_title.get_style_context().add_class("card-title")
        sink_text_box.pack_start(sink_title, False, False, 0)

        sink_subtitle = Gtk.Label(label="pipewire/wireplumber")
        sink_subtitle.set_xalign(0.0)
        sink_subtitle.get_style_context().add_class("card-subtitle")
        sink_text_box.pack_start(sink_subtitle, False, False, 0)

        # Device Dropdown
        self.sink_combo = Gtk.ComboBoxText()
        self.sink_combo.connect("changed", self.on_sink_changed)
        self.sink_card.pack_start(self.sink_combo, False, False, 0)

        # Volume Slider Box
        sink_slider_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.sink_card.pack_start(sink_slider_box, False, False, 0)

        self.sink_slider = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        self.sink_slider.set_draw_value(False)
        self.sink_slider.connect("value-changed", self.on_sink_volume_changed)
        self.sink_slider.connect("button-press-event", self.on_sink_slider_press)
        self.sink_slider.connect("button-release-event", self.on_sink_slider_release)
        sink_slider_box.pack_start(self.sink_slider, True, True, 0)

        self.sink_vol_label = Gtk.Label(label="50%")
        self.sink_vol_label.set_name("VolLabel")
        sink_slider_box.pack_start(self.sink_vol_label, False, False, 0)

        self.source_card = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.source_card.get_style_context().add_class("section")
        self.main_box.pack_start(self.source_card, False, False, 0)

        source_header = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10)
        self.source_card.pack_start(source_header, False, False, 0)

        source_icon = Gtk.Label(label="󰍬")
        source_icon.get_style_context().add_class("card-icon")
        source_header.pack_start(source_icon, False, False, 0)

        source_text_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=2)
        source_header.pack_start(source_text_box, True, True, 0)

        source_title = Gtk.Label()
        source_title.set_markup("<b>Ingresso audio</b>")
        source_title.set_xalign(0.0)
        source_title.get_style_context().add_class("card-title")
        source_text_box.pack_start(source_title, False, False, 0)

        source_subtitle = Gtk.Label(label="pipewire/wireplumber")
        source_subtitle.set_xalign(0.0)
        source_subtitle.get_style_context().add_class("card-subtitle")
        source_text_box.pack_start(source_subtitle, False, False, 0)

        # Device Dropdown
        self.source_combo = Gtk.ComboBoxText()
        self.source_combo.connect("changed", self.on_source_changed)
        self.source_card.pack_start(self.source_combo, False, False, 0)

        # Microphone Gain Slider Box
        source_slider_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
        self.source_card.pack_start(source_slider_box, False, False, 0)

        self.source_slider = Gtk.Scale.new_with_range(Gtk.Orientation.HORIZONTAL, 0, 100, 1)
        self.source_slider.set_draw_value(False)
        self.source_slider.connect("value-changed", self.on_source_volume_changed)
        self.source_slider.connect("button-press-event", self.on_source_slider_press)
        self.source_slider.connect("button-release-event", self.on_source_slider_release)
        source_slider_box.pack_start(self.source_slider, True, True, 0)

        self.source_vol_label = Gtk.Label(label="50%")
        self.source_vol_label.set_name("VolLabel")
        source_slider_box.pack_start(self.source_vol_label, False, False, 0)

    def on_draw(self, widget, cr):
        cr.set_operator(cairo.OPERATOR_CLEAR)
        cr.paint()
        cr.set_operator(cairo.OPERATOR_OVER)
        return False

    def load_styles(self):
        # Setup CSS provider
        css_provider = Gtk.CssProvider()
        
        # Load basic color palette if it exists
        color_css_path = os.path.expanduser("~/.config/colors/colors.css")
        if os.path.exists(color_css_path):
            with open(color_css_path, 'r') as f:
                css_content = f.read() + "\n"
        else:
            css_content = """
            @define-color background #2c4256;
            @define-color surface    #6384a3;
            @define-color foreground #f6f7fb;
            @define-color muted      #b9c4d2;
            @define-color accent     #8cb8e4;
            @define-color border     #809cb6;
            """
        
        # Custom Widget CSS
        css_content += """
        window,
        window.background,
        .background,
        decoration {
            background-color: transparent;
            background-image: none;
            box-shadow: none;
        }

        #MainContainer {
            background-color: alpha(@background, 0.60);
            border: 2px solid alpha(@border, 0.55);
            border-radius: 24px;
        }

        #HeaderTitle {
            font-family: "JetBrainsMono Nerd Font";
            font-size: 12pt;
            font-weight: bold;
            color: @accent;
        }

        .inputbar {
            background-color: @surface;
            border: 1px solid @border;
            border-radius: 14px;
            padding: 10px 14px;
        }

        .prompt {
            font-family: "JetBrainsMono Nerd Font";
            font-size: 12pt;
            font-weight: bold;
            color: @accent;
        }

        .section {
            background-color: transparent;
            border: 0;
            padding: 0;
        }

        .card-title {
            font-family: "JetBrainsMono Nerd Font";
            font-size: 11pt;
            font-weight: 600;
            color: @foreground;
        }

        .card-subtitle {
            font-family: "JetBrainsMono Nerd Font";
            font-size: 8.5pt;
            color: @muted;
        }

        .card-icon {
            font-size: 18pt;
            color: @accent;
        }

        #VolLabel {
            font-family: "JetBrainsMono Nerd Font";
            font-size: 9pt;
            color: @foreground;
            font-weight: bold;
            min-width: 35px;
        }

        /* Styling sliders (scales) */
        scale trough {
            background-color: alpha(@background, 0.48);
            border: 1px solid alpha(@border, 0.55);
            border-radius: 14px;
            min-height: 8px;
        }

        scale highlight {
            background-color: @accent;
            border-radius: 14px;
        }

        scale slider {
            background-color: @foreground;
            border: 1px solid @accent;
            border-radius: 14px;
            min-width: 12px;
            min-height: 12px;
            margin: -2px 0;
            box-shadow: none;
        }

        /* Styling comboboxes */
        combobox {
            background-color: transparent;
            color: @foreground;
            font-family: "JetBrainsMono Nerd Font";
            font-size: 11pt;
        }

        combobox button {
            background-image: none;
            background-color: transparent;
            border: 1px solid transparent;
            border-radius: 14px;
            padding: 10px 14px;
            box-shadow: none;
            color: @foreground;
        }

        combobox button:hover {
            background-color: @accent;
            color: @foreground;
        }

        combobox arrow {
            color: @accent;
        }

        combobox window {
            background-color: @background;
            border: 1px solid @border;
            border-radius: 14px;
        }

        combobox menu {
            background-color: @background;
            border: 1px solid @border;
            border-radius: 14px;
            color: @foreground;
        }
        """

        css_provider.load_from_data(css_content.encode('utf-8'))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            css_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    # =====================================================================
    # SLIDER / MOUSE DRAG DRIVERS
    # =====================================================================
    def on_sink_slider_press(self, widget, event):
        self.is_dragging_sink = True

    def on_sink_slider_release(self, widget, event):
        self.is_dragging_sink = False
        self.update_state()

    def on_source_slider_press(self, widget, event):
        self.is_dragging_source = True

    def on_source_slider_release(self, widget, event):
        self.is_dragging_source = False
        self.update_state()

    def on_sink_volume_changed(self, scale):
        val = int(scale.get_value())
        self.sink_vol_label.set_text(f"{val}%")
        if self.is_dragging_sink:
            # Set absolute volume using wpctl
            subprocess.run(["wpctl", "set-volume", "@DEFAULT_AUDIO_SINK@", f"{val/100:.2f}"])

    def on_source_volume_changed(self, scale):
        val = int(scale.get_value())
        self.source_vol_label.set_text(f"{val}%")
        if self.is_dragging_source:
            subprocess.run(["wpctl", "set-volume", "@DEFAULT_AUDIO_SOURCE@", f"{val/100:.2f}"])

    # =====================================================================
    # DROPDOWN COMBOBOX CHANGE EVENTS
    # =====================================================================
    def on_sink_changed(self, combo):
        if self.updating_sinks:
            return
        active_id = combo.get_active_id()
        if active_id:
            subprocess.run(["pactl", "set-default-sink", active_id])
            self.update_state()

    def on_source_changed(self, combo):
        if self.updating_sources:
            return
        active_id = combo.get_active_id()
        if active_id:
            subprocess.run(["pactl", "set-default-source", active_id])
            self.update_state()

    # =====================================================================
    # STATE UPDATE & BACKGROUND SYNC ENGINE
    # =====================================================================
    def get_sinks(self):
        # Returns: list of (name, desc, is_default)
        try:
            default_sink = subprocess.check_output(["pactl", "info"], text=True)
            def_match = re.search(r"Default Sink: (.*)", default_sink)
            def_sink = def_match.group(1).strip() if def_match else ""

            sinks_raw = subprocess.check_output(["pactl", "list", "sinks"], text=True)
            sinks = []
            current_name = ""
            current_desc = ""

            for line in sinks_raw.splitlines():
                if "Name:" in line:
                    current_name = line.split("Name:")[1].strip()
                elif "Description:" in line:
                    current_desc = line.split("Description:")[1].strip()
                    if current_name:
                        sinks.append((current_name, current_desc, current_name == def_sink))
                        current_name = ""
            return sinks
        except Exception:
            return []

    def get_sources(self):
        try:
            default_source = subprocess.check_output(["pactl", "info"], text=True)
            def_match = re.search(r"Default Source: (.*)", default_source)
            def_source = def_match.group(1).strip() if def_match else ""

            sources_raw = subprocess.check_output(["pactl", "list", "sources"], text=True)
            sources = []
            current_name = ""
            current_desc = ""

            for line in sources_raw.splitlines():
                if "Name:" in line:
                    current_name = line.split("Name:")[1].strip()
                elif "Description:" in line:
                    current_desc = line.split("Description:")[1].strip()
                    if current_name and not current_name.endswith(".monitor"):
                        sources.append((current_name, current_desc, current_name == def_source))
                        current_name = ""
            return sources
        except Exception:
            return []

    def update_state(self):
        # 1. Update Sink/Output ComboBox & Slider
        if not self.is_dragging_sink:
            try:
                vol_out = subprocess.check_output(["wpctl", "get-volume", "@DEFAULT_AUDIO_SINK@"], text=True).strip()
                parts = vol_out.split()
                if len(parts) >= 2:
                    vol_val = int(float(parts[1]) * 100)
                    self.sink_slider.set_value(vol_val)
                    self.sink_vol_label.set_text(f"{vol_val}%")
            except Exception:
                pass

        # Reload devices in combo boxes
        sinks = self.get_sinks()
        self.updating_sinks = True
        current_sink_items = [self.sink_combo.get_model()[i][0] for i in range(len(self.sink_combo.get_model()))] if self.sink_combo.get_model() else []
        new_sink_ids = [s[0] for s in sinks]

        if current_sink_items != new_sink_ids:
            self.sink_combo.remove_all()
            for name, desc, is_default in sinks:
                # Truncate descriptions if too long
                display_desc = desc[:DISPLAY_NAME_MAX] + "..." if len(desc) > DISPLAY_NAME_MAX else desc
                self.sink_combo.append(name, display_desc)

        for i, (name, desc, is_default) in enumerate(sinks):
            if is_default:
                self.sink_combo.set_active(i)
                break
        self.updating_sinks = False

        # 2. Update Source/Input ComboBox & Slider
        if not self.is_dragging_source:
            try:
                vol_out = subprocess.check_output(["wpctl", "get-volume", "@DEFAULT_AUDIO_SOURCE@"], text=True).strip()
                parts = vol_out.split()
                if len(parts) >= 2:
                    vol_val = int(float(parts[1]) * 100)
                    self.source_slider.set_value(vol_val)
                    self.source_vol_label.set_text(f"{vol_val}%")
            except Exception:
                pass

        sources = self.get_sources()
        self.updating_sources = True
        current_source_items = [self.source_combo.get_model()[i][0] for i in range(len(self.source_combo.get_model()))] if self.source_combo.get_model() else []
        new_source_ids = [s[0] for s in sources]

        if current_source_items != new_source_ids:
            self.source_combo.remove_all()
            for name, desc, is_default in sources:
                display_desc = desc[:DISPLAY_NAME_MAX] + "..." if len(desc) > DISPLAY_NAME_MAX else desc
                self.source_combo.append(name, display_desc)

        for i, (name, desc, is_default) in enumerate(sources):
            if is_default:
                self.source_combo.set_active(i)
                break
        self.updating_sources = False

        return True  # Keep GLib timer running

    # =====================================================================
    # WINDOW KEY HANDLER
    # =====================================================================
    def on_key_press(self, widget, event):
        if event.keyval == Gdk.KEY_Escape:
            Gtk.main_quit()

    def on_focus_out(self, widget, event):
        if self.can_close_on_focus_loss:
            Gtk.main_quit()

    def enable_focus_close(self):
        self.can_close_on_focus_loss = True
        return False

if __name__ == "__main__":
    lock = acquire_single_instance_lock()
    if lock is None:
        sys.exit(0)

    app = AudioControlCenter()
    app.show_all()
    Gtk.main()
