"""Exercise desktop preference routing and the real staged boot writer privately."""
from pathlib import Path
import hashlib
import os
import subprocess
import sys
import tempfile

support = Path(__file__).resolve().parents[1]
core = str(Path(sys.argv[1]).resolve())


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


with tempfile.TemporaryDirectory(prefix="anto-boot-flow-") as directory:
    root = Path(directory)
    scripts = root / "scripts"
    scripts.mkdir()
    for name in ("wallpaper_effects_modules.sh", "wallpaper_boot_apply.sh", "wallpaper_boot_commit.sh", "boot_render.py"):
        (scripts / name).symlink_to(support / "compat" / name)
    (scripts / "wallpaper_effects.d").symlink_to(support / "wallpaper_effects.d", target_is_directory=True)
    (scripts / "wallpaper_core").symlink_to(core)
    bin_dir = root / "bin"
    bin_dir.mkdir()
    for name, content in {
        "hyprctl": "#!/bin/sh\nprintf '[]\\n'\n",
        "notify-send": "#!/bin/sh\nprintf 'notified\\n' >> \"$FIXTURE_ROOT/notifications\"\n",
    }.items():
        executable = bin_dir / name
        executable.write_text(content)
        executable.chmod(0o700)
    local = root / "config/anto426-local"
    owner = root / "cache/awww/current-wallpaper.path"
    owner.parent.mkdir(parents=True)
    grub = root / "system/grub"
    sddm = root / "system/sddm"
    grub.mkdir(parents=True)
    (sddm / "Backgrounds").mkdir(parents=True)
    env = dict(os.environ, HOME=str(root / "home"), XDG_CONFIG_HOME=str(root / "config"),
               XDG_CACHE_HOME=str(root / "cache"), XDG_STATE_HOME=str(root / "state"),
               ANTO_LOCAL_CONFIG_ROOT=str(local), ANTO426_SCRIPT_DIR=str(scripts),
               ANTO426_WALLPAPER_CORE=core, ANTO426_GRUB_THEME_DIR=str(grub),
               ANTO426_SDDM_THEME_DIR=str(sddm), ANTO426_WALLPAPER_CORE_MODULES="sync",
               ANTO426_WALLPAPER_CORE_APPS="0", ANTO426_WALLPAPER_CORE_ICONS="0",
               ANTO426_WALLPAPER_CORE_VSCODE="0", ANTO426_WALLPAPER_CORE_BOOT="1",
               FIXTURE_ROOT=str(root), PATH=f"{bin_dir}:{os.environ['PATH']}")

    def run(*args, success=True):
        result = subprocess.run(args, env=env, timeout=20, capture_output=True, text=True)
        assert (result.returncode == 0) == success, result.stderr
        return result

    first = root / "first ' wallpaper.png"
    second = root / "second.png"
    run("magick", "-size", "48x32", "xc:#d02070", str(first))
    run("magick", "-size", "48x32", "xc:#2080d0", str(second))
    owner.write_text(str(first) + "\n")
    run(core, "effects", str(first))
    assert (grub / "theme.txt").is_file(), "The desktop bridge omitted the boot phase"
    assert (sddm / "Backgrounds/anto426-current.png").is_file()
    before = digest(grub / "background.jpg")
    boot_state = local / "wallpaper/boot-login.env"
    assert str(first) in boot_state.read_text().replace("'\\''", "'")

    env["ANTO426_WALLPAPER_CORE_BOOT"] = "0"
    owner.write_text(str(second) + "\n")
    run(core, "effects", str(second))
    assert digest(grub / "background.jpg") == before, "Disabled boot preference was ignored"
    palette = local / "theme/colors.css"
    palette_before = digest(palette)
    # Explicit boot-only selection works even when automatic updates are off.
    run(str(scripts / "wallpaper_boot_apply.sh"), "--current")
    assert digest(grub / "background.jpg") != before
    assert str(second) in boot_state.read_text()
    assert digest(palette) == palette_before
    assert owner.read_text().strip() == str(second)

    # All system paths are checked before replacing the first theme asset.
    installed = digest(grub / "background.jpg")
    env["ANTO426_SDDM_THEME_DIR"] = str(root / "missing-system-theme")
    run(str(scripts / "wallpaper_boot_apply.sh"), str(first), success=False)
    assert digest(grub / "background.jpg") == installed
    print("boot menu: automatic on/off, recursive domain locks, explicit selection, desktop isolation and commit preflight verified")
