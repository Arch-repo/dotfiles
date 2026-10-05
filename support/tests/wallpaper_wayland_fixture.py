"""Check legacy preview sampling using synthetic images in a nested compositor."""
from datetime import datetime
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import time
from PIL import Image

root = Path(__file__).resolve().parents[2]
user_home = Path.home()


def run(*args, env=None):
    return subprocess.check_output([str(x) for x in args], env=env, text=True)


original = json.loads(run("hyprctl", "-j", "activeworkspace"))["name"]
occupied = {item["id"] for item in json.loads(run("hyprctl", "-j", "workspaces")) if item["windows"]}
workspace = next(item for item in range(51, 80) if item not in occupied)
with tempfile.TemporaryDirectory(prefix="anto-preview-wayland-") as directory:
    folder = Path(directory)
    home = folder / "home"
    assets = home / ".local/share/anto-desktop"
    assets.mkdir(parents=True)
    for css in (user_home / ".local/share/anto-desktop").glob("*.css"):
        shutil.copyfile(css, assets / css.name)
    env_file = folder / "environment.json"
    save = folder / "saveenv.py"
    save.write_text("import json,os,pathlib\npathlib.Path(" + repr(str(env_file)) +
                    ").write_text(json.dumps({key:os.environ.get(key,'') for key in "
                    "['WAYLAND_DISPLAY','HYPRLAND_INSTANCE_SIGNATURE','DBUS_SESSION_BUS_ADDRESS']}))\n")
    config = folder / "hyprland.conf"
    config.write_text("monitor = , 1280x800, 0x0, 1\nanimations {\n enabled = false\n}\n"
                      f"source = {user_home}/.config/anto426-local/hypr/glass-loader.conf\n"
                      f"source = {root}/.config/hypr/conf/glass.conf\n"
                      f"exec-once = python3 {save}\n")
    base = dict(os.environ, HOME=str(home), XDG_CONFIG_HOME=str(folder / "config"),
                XDG_CACHE_HOME=str(folder / "cache"), XDG_STATE_HOME=str(folder / "state"),
                XDG_DATA_HOME=str(folder / "data"), ANTO_LOCAL_CONFIG_ROOT=str(folder / "config/anto426-local"),
                HYPRLAND_NO_RT="1")
    compositor = None
    run("hyprctl", "dispatch", "workspace", str(workspace))
    with (folder / "runtime.log").open("w") as log:
        try:
            compositor = subprocess.Popen(["dbus-run-session", "--", "Hyprland", "--config", str(config)],
                                           env=base, stdout=log, stderr=log)
            deadline = time.monotonic() + 12
            while not env_file.exists() and compositor.poll() is None and time.monotonic() < deadline:
                time.sleep(.1)
            assert env_file.exists(), "Nested compositor did not start"
            child = dict(base, **json.loads(env_file.read_text()), GDK_BACKEND="wayland", GTK_A11Y="none",
                         ANTO426_WALLPAPER_FIXTURE_CAPTURE=str(folder))

            def hypr(*args):
                return run("hyprctl", "-i", child["HYPRLAND_INSTANCE_SIGNATURE"], *args, env=child)

            time.sleep(.5)
            assert not hypr("configerrors").strip()
            monitors = json.loads(hypr("-j", "monitors"))
            connector = monitors[-1]["name"]
            for monitor in monitors[:-1]:
                hypr("output", "remove", monitor["name"])
            child["ANTO426_TARGET_MONITOR"] = connector
            hypr("keyword", "plugin:hyprglass:debug:timers", "1")
            hypr("hyprglass", "stats", "reset")
            result = subprocess.run([str(root / "build/wallpaper-menu-fixture"), str(user_home / ".local/bin/anto-wallpaper")],
                                     env=child, capture_output=True, text=True, timeout=20)
            assert result.returncode == 0, result.stdout + result.stderr
            assert "WARNING" not in result.stderr and "CRITICAL" not in result.stderr, result.stderr
            first = Image.open(folder / "legacy-preview-a.png").convert("RGB")
            second = Image.open(folder / "legacy-preview-b.png").convert("RGB")
            outside = [first.getpixel((10, 10)), second.getpixel((10, 10))]
            glass = [first.getpixel((240, 180)), second.getpixel((240, 180))]
            assert max(abs(outside[0][i] - outside[1][i]) for i in range(3)) > 35, outside
            assert max(abs(glass[0][i] - glass[1][i]) for i in range(3)) > 15, glass
            stats = json.loads(hypr("-j", "hyprglass", "stats"))
            assert sum(m["layerCacheMisses"] for m in stats["monitors"]) > 2, stats
            names = [item["namespace"] for monitor in json.loads(hypr("-j", "layers")).values()
                     for level in monitor.get("levels", {}).values() for item in level]
            assert "anto426-wallpaper-preview" not in names, "Visual preview survived page/window disposal"
            report = dict(verified_at=datetime.now().astimezone().isoformat(), passed=True,
                          session="nested-wayland", personal_data=False, outside_pixels=outside,
                          glass_pixels=glass, glass_stats=stats,
                          preview_removed_on_close=True, fixture_output=result.stdout.strip())
            (root / "docs/wallpaper-menu-verification.json").write_text(json.dumps(report, indent=2) + "\n")
            shutil.copyfile(folder / "legacy-preview-b.png", root / "docs/screenshots/wallpaper.png")
            print(json.dumps({k: report[k] for k in ["passed", "outside_pixels", "glass_pixels", "preview_removed_on_close"]}))
        finally:
            if compositor is not None:
                compositor.terminate()
                try:
                    compositor.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    compositor.kill()
                    compositor.wait()
            # Do not undo a workspace change made by the user during the test.
            current = json.loads(run("hyprctl", "-j", "activeworkspace"))["id"]
            if current == workspace:
                run("hyprctl", "dispatch", "workspace", original)
