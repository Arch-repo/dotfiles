#!/usr/bin/env python3
"""Verify real PipeWire/CAVA capture on a private sink without speaker output."""
from datetime import datetime
from pathlib import Path
import json
import math
import os
import selectors
import struct
import subprocess
import tempfile
import time
import wave

root = Path(__file__).resolve().parents[1]
sink = 'anto_widget_audio_probe_' + str(os.getpid())
module = subprocess.check_output(['pactl', 'load-module', 'module-null-sink',
                                 'sink_name=' + sink], text=True).strip()
player = analyzer = None
try:
    with tempfile.TemporaryDirectory(prefix='anto-widget-audio-') as temporary:
        folder = Path(temporary)
        tone = folder / 'tone.wav'
        rate = 48000
        with wave.open(str(tone), 'wb') as audio:
            audio.setnchannels(2)
            audio.setsampwidth(2)
            audio.setframerate(rate)
            data = bytearray()
            for i in range(rate * 4):
                value = int(3000 * sum(math.sin(2 * math.pi * f * i / rate)
                                      for f in (80, 220, 440, 1200)))
                data.extend(struct.pack('<hh', value, value))
            audio.writeframes(data)
        config = folder / 'cava.conf'
        native = Path(os.environ['XDG_RUNTIME_DIR']) / 'anto426-native-widgets/spectrum.conf'
        config.write_text(native.read_text().replace('source=auto', 'source=' + sink + '.monitor'))
        analyzer = subprocess.Popen(['cava', '-p', str(config)], stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
        player = subprocess.Popen(['pw-play', '--target=' + sink, str(tone)],
                                  stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        selector = selectors.DefaultSelector()
        selector.register(analyzer.stdout, selectors.EVENT_READ)
        raw = b''
        until = time.monotonic() + 2
        while time.monotonic() < until:
            if selector.select(.05):
                raw += analyzer.stdout.read1(32768)
        frames = [[int(v) for v in line.split(b';') if v] for line in raw.splitlines()]
        assert frames and all(len(frame) == 32 for frame in frames), 'Invalid raw audio frames'
        peak = max(max(frame) for frame in frames)
        assert peak > 100, 'No signal captured from the private output'
        report = dict(verified_at=datetime.now().astimezone().isoformat(), passed=True,
                      backend='pipewire', private_sink=True, speaker_output=False,
                      frames=len(frames), bars=32, peak=peak,
                      note='Synthetic signal on an isolated sink; KDE Connect remote audio is not local PCM')
        (root / 'docs/widgets-audio-verification.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report))
finally:
    for process in (player, analyzer):
        if process:
            if process.poll() is None:
                process.terminate()
            process.wait(timeout=3)
    subprocess.run(['pactl', 'unload-module', module], check=True)
