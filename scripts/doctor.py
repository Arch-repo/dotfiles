#!/usr/bin/env python3
"""Check build versions, feature dependencies and installed artifact identity."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()
FEATURES = {
    "desktop": ["hyprctl", "waybar", "swaync-client", "ghostty", "notify-send", "timeout"],
    "wallpaper": ["awww", "awww-daemon", "mpvpaper", "ffmpeg", "magick", "jq"],
    "network": ["nmcli", "bluetoothctl", "pactl", "wpctl"],
    "capture": ["grim", "slurp", "wl-copy", "tesseract", "wf-recorder"],
    "clipboard": ["wl-paste", "cliphist"],
    "media": ["playerctl", "brightnessctl"],
    "calendar": ["curl", "dbus-monitor"],
    "widgets": ["cava"],
    "authentication": ["sddm-greeter-qt6", "hyprlock", "fprintd-list"],
    "application-themes": ["node", "python3"],
}

def main():
    dependencies = {feature: {name: shutil.which(name) is not None for name in names} for feature, names in FEATURES.items()}
    artifacts = {}
    binaries = {
        "anto-menu": "bin", "anto-wallpaper": "bin", "anto-osd": "bin",
        "anto-menu-backend": "libexec/anto-menu", "anto-wallpaper-core": "libexec/anto426",
        "anto-widgets": "libexec/anto426", "anto-calendar": "libexec/anto426",
    }
    for binary, directory in binaries.items():
        source, installed = ROOT / "build" / binary, HOME / ".local" / directory / binary
        artifacts[binary] = source.is_file() and installed.is_file() and hashlib.sha256(source.read_bytes()).digest() == hashlib.sha256(installed.read_bytes()).digest()
    styles = {}
    for source in sorted((ROOT / 'build/design').glob('*')):
        if source.suffix not in ('.css','.conf') and source.name != 'application-material.json':
            continue
        installed = HOME / '.local/share/anto-desktop' / source.name
        styles[source.name] = installed.is_file() and source.read_bytes() == installed.read_bytes()
    versions = {}
    for package in ("gtk4", "gtk4-layer-shell-0", "gio-2.0", "json-c", "wayland-client", "hyprland"):
        result = subprocess.run(["pkg-config", "--modversion", package], text=True, capture_output=True)
        versions[package] = result.stdout.strip() if result.returncode == 0 else None
    for language in ("ita", "eng"):
        dependencies.setdefault("ocr-models", {})[language] = (HOME / f".local/share/anto-desktop/tessdata/{language}.traineddata").is_file()
    font = subprocess.run(["fc-match", "-f", "%{family}", "Noto Color Emoji"], text=True, capture_output=True) if shutil.which("fc-match") else None
    dependencies["emoji-data"] = {"native-unicode-catalog": (HOME/".local/share/anto-desktop/emoji.tsv").is_file()}
    resources = Path(os.environ.get("XDG_DATA_HOME", HOME / ".local/share")) / "anto-desktop"
    for name, theme in json.loads((ROOT / "resources.lock.json").read_text()).get("app_themes", {}).items():
        checks = {}
        for item in theme["files"]:
            target = resources / name / item["filename"]
            checks[item["filename"]] = target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == item["sha256"]
        dependencies[name] = checks
    dependencies["fonts"] = {"noto-color-emoji": font is not None and font.returncode == 0 and "Noto Color Emoji" in font.stdout}
    terminal = subprocess.run(['ghostty','+show-config'],text=True,capture_output=True)
    terminal_values = dict(line.split(' = ',1) for line in terminal.stdout.splitlines() if ' = ' in line)
    opacity = json.loads((ROOT / 'design/tokens.json').read_text())['material']['opacity']
    terminal_shared = terminal.returncode == 0 and terminal_values.get('background-opacity') == str(opacity) and terminal_values.get('background-opacity-cells') == 'true'
    errors = subprocess.run(["hyprctl", "configerrors"], text=True, capture_output=True)
    glass = {}
    receipt = HOME / ".local/share/anto-desktop/hyprglass-build.json"
    library = HOME / ".local/lib/hyprland/hyprglass.so"
    if receipt.is_file() and library.is_file():
        saved = json.loads(receipt.read_text())
        pinned = json.loads((ROOT / "resources.lock.json").read_text())["glass"]
        glass["binary_verified"] = saved.get("sha256") == hashlib.sha256(library.read_bytes()).hexdigest()
        glass["source_pinned"] = saved.get("revision") == pinned["revision"]
        glass["patch_verified"] = saved.get("patch_sha256") == hashlib.sha256((ROOT / pinned["patch"]).read_bytes()).hexdigest()
        running = subprocess.run(["hyprctl", "-j", "version"], text=True, capture_output=True)
        glass["compositor_matches_build"] = running.returncode == 0 and json.loads(running.stdout).get("commit") == saved.get("hyprland_revision")
        loaded = subprocess.run(["hyprctl", "plugin", "list"], text=True, capture_output=True)
        glass["loaded"] = loaded.returncode == 0 and "Plugin hyprglass by" in loaded.stdout
    report = dict(versions=versions, dependencies=dependencies, installed_matches_build=artifacts, installed_styles_match_build=styles, terminal_material_shared=terminal_shared, glass=glass, hyprland_config_valid=errors.returncode == 0 and not errors.stdout.strip())
    print(json.dumps(report, indent=2))
    return 0 if all(artifacts.values()) and styles and all(styles.values()) and terminal_shared and all(all(items.values()) for items in dependencies.values()) and glass and all(glass.values()) and report["hyprland_config_valid"] else 1

if __name__ == "__main__":
    raise SystemExit(main())
