import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def await_file(path, seconds=5):
    deadline = time.monotonic() + seconds
    while not path.exists() and time.monotonic() < deadline:
        time.sleep(0.02)
    assert path.exists(), f"Worker did not create {path.name}"


with tempfile.TemporaryDirectory(prefix="anto-background-") as temporary:
    root = Path(temporary)
    worker = root / "backend"
    worker.write_text("""#!/usr/bin/env python3
import json, os, pathlib, sys, time
root = pathlib.Path(os.environ['FIXTURE_ROOT'])
(root / 'started').write_text(json.dumps([sys.argv[1:], os.environ['ANTO426_WALLPAPER_OUTPUT']]))
deadline = time.monotonic() + 8
while not (root / 'release').exists() and time.monotonic() < deadline:
    time.sleep(.02)
(root / 'completed').touch()
sys.exit(int(os.environ.get('FIXTURE_FAILURE', '0')))
""")
    worker.chmod(0o700)
    notify = root / "notify-send"
    notify.write_text("#!/usr/bin/env python3\nimport os, pathlib\npathlib.Path(os.environ['FIXTURE_ROOT'], 'notified').touch()\n")
    notify.chmod(0o700)
    image = root / "sfondo ' $(literal).png"
    image.touch()
    environment = dict(os.environ, XDG_STATE_HOME=str(root / "state"), FIXTURE_ROOT=str(root),
                       ANTO426_WALLPAPER_WORKER=str(Path(sys.argv[1]).resolve()),
                       ANTO426_WALLPAPER_CORE=str(worker), PATH=f"{root}:{os.environ['PATH']}")
    try:
        subprocess.run([sys.argv[1], "launch", str(image), "eDP-1"], env=environment, check=True, timeout=1)
        await_file(root / "started")
        assert not (root / "completed").exists(), "Launcher waited for wallpaper generation"
        assert json.loads((root / "started").read_text()) == [["apply", str(image)], "eDP-1"]
        (root / "release").touch()
        await_file(root / "completed")
        # Failure must remain visible after the original launching process exits.
        (root / "completed").unlink()
        environment["FIXTURE_FAILURE"] = "1"
        subprocess.run([sys.argv[1], "launch", str(image), "ALL"], env=environment, check=True, timeout=1)
        await_file(root / "completed")
        await_file(root / "notified")
        assert "Sfondo" in (root / "state/anto426/wallpaper-jobs.log").read_text()
    finally:
        (root / "release").touch()
