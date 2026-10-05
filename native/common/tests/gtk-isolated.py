#!/usr/bin/env python3
"""Run a GTK fixture without touching the user's display or data."""
from pathlib import Path
import os
import argparse
import shutil
import subprocess
import sys
import tempfile
import time

parser = argparse.ArgumentParser()
parser.add_argument("--assets", type=Path)
parser.add_argument("--tokens", type=Path)
parser.add_argument("command", nargs=argparse.REMAINDER)
options = parser.parse_args()
with tempfile.TemporaryDirectory(prefix="anto-gtk-fixture-") as folder:
    root = Path(folder)
    home = root / "home"
    home.mkdir()
    if options.assets:
        assets = home / ".local/share/anto-desktop"
        assets.mkdir(parents=True)
        for source in options.assets.glob("*.css"):
            shutil.copyfile(source, assets / source.name)
        shutil.copyfile(options.tokens, assets / "tokens.css")
    environment = dict(os.environ, GDK_BACKEND="broadway", BROADWAY_DISPLAY=":1",
                       GTK_A11Y="none", GIO_USE_VFS="local", GDK_DEBUG="no-portals", GTK_THEME="Adwaita:dark", GSK_RENDERER="cairo",
                       HOME=str(home), XDG_RUNTIME_DIR=folder, XDG_DATA_HOME=str(root / "data"),
                       XDG_STATE_HOME=str(root / "state"),
                       XDG_CONFIG_HOME=str(root / "config"), XDG_CACHE_HOME=str(root / "cache"))
    server = subprocess.Popen(["gtk4-broadwayd", "-a", "127.0.0.1", "-p", "0", ":1"],
                              env=environment, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    try:
        for _ in range(100):
            if (root / "broadway1.socket").exists():
                break
            assert server.poll() is None, server.stderr.read().decode()
            time.sleep(.01)
        result = subprocess.run(options.command, env=environment, timeout=20, capture_output=True, text=True)
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        assert "Gtk-WARNING" not in result.stderr and "CRITICAL" not in result.stderr
        sys.exit(result.returncode)
    finally:
        server.terminate()
        server.wait(timeout=3)
