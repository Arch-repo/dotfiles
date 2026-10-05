"""Exercise the production single-owner CLI on a private bus and GTK display."""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import json
import os
import subprocess
import sys
import time

binary = Path(sys.argv[1]).resolve()
root = Path(os.environ['XDG_CONFIG_HOME']) / 'anto426-local'
state = root / 'widgets/state.json'
# Test DSP producer: lifecycle checks do not require the user's PipeWire session.
bin_dir = Path(os.environ['HOME']) / 'bin'
bin_dir.mkdir()
cava = bin_dir / 'cava'
cava.write_text('#!/usr/bin/python3\nimport time\nwhile True:\n print(";".join(["250"]*32),flush=True)\n time.sleep(.04)\n')
cava.chmod(0o700)
os.environ['PATH'] = str(bin_dir) + ':' + os.environ['PATH']
log = Path(os.environ['HOME']) / 'widgets.log'

def cli(*args):
    return subprocess.check_output([str(binary), *args], text=True, timeout=3).strip()

def wait_for(predicate):
    until = time.monotonic() + 3
    while time.monotonic() < until:
        if predicate():
            return
        time.sleep(.025)
    raise AssertionError('Widget lifecycle condition timed out')

def config():
    return json.loads(state.read_text())

def children(pid):
    path = Path(f'/proc/{pid}/task/{pid}/children')
    return set(path.read_text().split()) if path.exists() else set()

process = None
with log.open('w') as output:
    try:
        assert cli('status') == 'disabled'
        cli('enable')
        process = subprocess.Popen([str(binary), 'start'], stdout=output, stderr=output)
        wait_for(lambda: cli('status') == 'running')
        wait_for(lambda: len(children(process.pid)) == 1)
        dsp = children(process.pid)
        cli('start')
        assert process.poll() is None
        assert children(process.pid) == dsp
        cli('unlock')
        wait_for(lambda: not config()['locked'])
        cli('lock')
        wait_for(lambda: config()['locked'])
        assert children(process.pid) == dsp, 'Position locking restarted DSP'
        # Multiple clients must update the owner's current state, not old snapshots.
        with ThreadPoolExecutor(2) as pool:
            list(pool.map(lambda name: cli('set-visible', name, '0'), ['clock', 'calendar']))
        wait_for(lambda: not config()['widgets']['clock'] and not config()['widgets']['calendar'])
        with ThreadPoolExecutor(2) as pool:
            list(pool.map(lambda name: cli('set-visible', name, '1'), ['clock', 'calendar']))
        wait_for(lambda: config()['widgets']['clock'] and config()['widgets']['calendar'])
        cli('set-autostart', '0')
        wait_for(lambda: not config()['autostart'])
        cli('stop')
        process.wait(timeout=3)
        assert cli('status') == 'stopped'
        assert cli('autostart') == ''
        assert cli('status') == 'stopped'
        # Offline concurrent edits serialize the read as well as the write.
        with ThreadPoolExecutor(2) as pool:
            list(pool.map(lambda name: cli('set-visible', name, '0'), ['clock', 'calendar']))
        assert not config()['widgets']['clock'] and not config()['widgets']['calendar']
        process = subprocess.Popen([str(binary), 'start'], stdout=output, stderr=output)
        wait_for(lambda: cli('status') == 'running')
        cli('disable')
        process.wait(timeout=3)
        assert cli('status') == 'disabled'
        assert not children(process.pid)
    finally:
        if process and process.poll() is None:
            process.terminate()
            process.wait(timeout=3)
messages = log.read_text()
assert 'WARNING' not in messages and 'CRITICAL' not in messages, messages
print('native widget lifecycle: singleton, parallel visibility edits, geometry-only lock, one DSP, offline serialization, autostart and teardown passed')
