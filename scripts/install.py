#!/usr/bin/env python3
"""Build and install the native desktop; import only approved user settings."""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
from glass import install as install_glass

ROOT = Path(__file__).resolve().parent.parent
HOME = Path.home()
CONFIG = HOME / ".config"
LOCAL = HOME / ".local"

def run(*arguments):
    subprocess.run([str(item) for item in arguments], check=True, cwd=ROOT)

def link(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.is_symlink() and destination.resolve() == source.resolve():
        return
    if destination.exists() and destination.is_dir() and not destination.is_symlink():
        raise RuntimeError(f"Directory conflict: {destination}")
    temporary = destination.with_name(destination.name + ".install-link")
    temporary.unlink(missing_ok=True)
    temporary.symlink_to(source)
    temporary.replace(destination)

def import_missing(source, destination):
    """No links, binaries, credentials or overwrites from the historical tree."""
    if source.is_symlink():
        return
    if source.is_dir():
        destination.mkdir(parents=True, exist_ok=True)
        for child in source.iterdir():
            if child.name != ".locks":
                import_missing(child, destination / child.name)
    elif source.is_file() and not destination.exists():
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

def static_tree(source, destination, mutable=False):
    if source.is_dir():
        destination.mkdir(parents=True, exist_ok=True)
        for child in source.iterdir():
            static_tree(child, destination / child.name, mutable)
    elif source.is_file():
        if mutable:
            import_missing(source, destination)
        else:
            link(source, destination)

def install_terminal_material():
    """Manage only material keys; retain terminal font, keys and user settings."""
    config = CONFIG / 'ghostty/config'
    include = '~/.local/share/anto-desktop/terminal.conf'
    lines = config.read_text().splitlines()
    retained = []
    for line in lines:
        if re.match(r'\s*(background-opacity(-cells)?|background-blur)\s*=', line):
            continue
        if re.match(r'\s*config-file\s*=', line) and line.split('=',1)[1].strip().strip('\"\'') == include:
            continue
        retained.append(line)
    text = '\n'.join(retained).rstrip() + f'\nconfig-file = {include}\n'
    if text != config.read_text():
        temporary = config.with_suffix('.installing')
        temporary.write_text(text)
        temporary.chmod(config.stat().st_mode & 0o777)
        temporary.replace(config)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--restore", type=Path, help="Existing historical backup, read only")
    parser.add_argument("--skip-build", action="store_true")
    options = parser.parse_args()
    state = CONFIG / "anto426-local"
    if options.restore:
        backup = options.restore.resolve()
        if not (backup / "original-dotfiles").is_dir():
            parser.error("The backup must contain original-dotfiles")
        for name in ("hypr", "widgets", "theme", "ghostty", "display", "notes", "wallpaper"):
            import_missing(backup / "external/.config/anto426-local" / name, state / name)
        for name in ("calendar", "notifications", "wifi", "sync.env"):
            import_missing(backup / "external/.local/share/anto426" / name, LOCAL / "share/anto426" / name)
        for name in ("gtk-3.0", "gtk-4.0", "qt5ct", "qt6ct", "Kvantum"):
            import_missing(backup / "external/.config" / name, CONFIG / name)
        import_missing(backup / "external/.themes/anto426", HOME / ".themes/anto426")
    for name in ("hypr", "widgets", "theme", "ghostty", "wallpaper", "notes"):
        (state / name).mkdir(parents=True, exist_ok=True)
    current = CONFIG / "hypr/hyprland.conf"
    monitors = state / "hypr/monitors.conf"
    # Preserve the geometry of the usable reset session, including mirror mode.
    if current.is_file() and not current.is_symlink():
        lines = [line for line in current.read_text().splitlines() if line.strip().startswith(("monitor=", "monitor ="))]
        if lines:
            monitors.write_text("\n".join(lines) + "\n")
    if not monitors.exists():
        monitors.write_text("monitor = , preferred, auto, 1\n")
    for name in ("workspaces.conf", "widget-apps.generated.conf", "widget-lock.generated.conf"):
        (state / "hypr" / name).touch(exist_ok=True)
    preferences = state / "wallpaper/preferences.json"
    if not preferences.exists():
        preferences.write_text(json.dumps(dict(apps=True, vscode=True, obsidian=True, icons=True, boot=True), indent=2) + "\n")
        preferences.chmod(0o600)
    palette = state / "theme/colors.css"
    import_missing(ROOT / "native/menu/assets/tokens.css", palette)
    theme = state / "hypr/theme.generated.conf"
    if not theme.exists():
        from design import generate
        generate(ROOT / "build/design")
        theme.write_text((ROOT / "build/design/hyprlock-defaults.conf").read_text())
    (state / "ghostty/dynamic.conf").touch(exist_ok=True)
    if not options.skip_build:
        run("cmake", "-S", ROOT, "-B", ROOT / "build", "-G", "Ninja", "-DCMAKE_BUILD_TYPE=RelWithDebInfo", f"-DCMAKE_INSTALL_PREFIX={LOCAL}")
        run("cmake", "--build", ROOT / "build", "-j", "6")
        run("ctest", "--test-dir", ROOT / "build", "--output-on-failure")
    run("cmake", "--install", ROOT / "build")
    install_glass()
    for obsolete in ('glass.vert','glass.frag'):
        (LOCAL / 'share/anto-desktop' / obsolete).unlink(missing_ok=True)
    # Generated application themes live in real user directories, outside Git.
    for source in (ROOT / ".config").iterdir():
        mutable = source.name in ("gtk-3.0", "gtk-4.0", "Kvantum", "btop", "ghostty")
        if source.name not in ("hypr", "systemd"):
            static_tree(source, CONFIG / source.name, mutable)
    install_terminal_material()
    static_tree(ROOT / ".config/hypr", CONFIG / "hypr")
    static_tree(ROOT / ".config/systemd/user", CONFIG / "systemd/user")
    support = CONFIG / "anto426"
    support.mkdir(parents=True, exist_ok=True)
    for source in (ROOT / "support/compat").iterdir():
        link(source, support / source.name)
    for name in ("wallpaper_effects.d", "lib"):
        link(ROOT / "support" / name, support / name)
    for legacy, binary in dict(wallpaper_core="anto-wallpaper-core", widgets_core="anto-widgets", remote_sync_core="anto-calendar", keyboard_status_json="anto-keyboard-status").items():
        link(LOCAL / "libexec/anto426" / binary, support / legacy)
    link(LOCAL / "libexec/anto-menu/anto-menu-backend", LOCAL / "bin/anto-config")
    link(ROOT / "README.md", support / "README.md")
    if (ROOT / ".tmux/plugins/tpm/tpm").is_file():
        link(ROOT / ".tmux/plugins/tpm", HOME / ".tmux/plugins/tpm")
    for filename in (".zshrc", ".tmux.conf"):
        link(ROOT / filename, HOME / filename)
    for program, page in (("anto-desktop", "system"), ("anto-wallpaper-gallery", None)):
        target = LOCAL / "share/applications" / (program + ".desktop")
        target.parent.mkdir(parents=True, exist_ok=True)
        command = LOCAL / "bin" / ("anto-menu" if page else "anto-wallpaper")
        target.write_text(f"[Desktop Entry]\nType=Application\nName={'Anto Desktop' if page else 'Sfondi'}\nExec={command}{' system' if page else ''}\nIcon=preferences-system-symbolic\nCategories=Settings;DesktopSettings;\nTerminal=false\n")
    run("systemctl", "--user", "daemon-reload")
    run(LOCAL / "bin/anto-menu", "workspace-output", "sync")
    run(LOCAL / "libexec/anto-menu/anto-menu-backend", "notes", "init")
    print("Installed native desktop and imported user settings. No new backup created.")

if __name__ == "__main__":
    main()
