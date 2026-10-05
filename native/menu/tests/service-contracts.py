#!/usr/bin/env python3
"""Reject invalid calls at the service boundary before any provider is entered."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

backend = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="anto-contracts-") as temporary:
    env = dict(os.environ, ANTO_MENU_DRY_RUN="1", ANTO_MENU_NO_NOTIFY="1",
               ANTO_LOCAL_CONFIG_ROOT=temporary + "/settings")

    def invoke(*args):
        return subprocess.run([str(backend), *args], env=env, text=True,
                              capture_output=True, timeout=3)

    description = invoke("--describe")
    assert description.returncode == 0, description.stderr
    contract = json.loads(description.stdout)
    assert contract["version"] == 1
    services = contract["services"]
    assert set(services) == {"audio", "bluetooth", "calendar", "capture", "config",
                             "display", "energy", "network", "notes", "record",
                             "session", "system", "keyboard", "floating", "background",
                             "notifications"}
    rejected = 0
    for domain, operations in services.items():
        assert len({item["name"] for item in operations}) == len(operations), domain
        invalid = invoke(domain, "unknown-operation")
        assert invalid.returncode == 2 and "invalid-operation" in invalid.stderr, domain
        for operation in operations:
            low, high = operation["minArgs"], operation["maxArgs"]
            assert 0 <= low <= high and isinstance(operation["mutation"], bool), operation
            counts = [high + 1] + ([low - 1] if low else [])
            for count in counts:
                result = invoke(domain, operation["name"], *(["unused"] * count))
                assert result.returncode == 2, (domain, operation, result.stderr)
                assert "invalid-arguments" in result.stderr and not result.stdout, (domain, operation)
                rejected += 1

    for value in ("nan", "inf", "-inf", "1.6", "-0.1", "1;echo test", ""):
        result = invoke("audio", "volume", "@DEFAULT_AUDIO_SINK@", value)
        assert result.returncode == 2 and "invalid-volume" in result.stderr, value
    for value in ("nan", "inf", "-inf", "0", "101", "", "1;echo test"):
        result = invoke("energy", "brightness-set", value)
        assert result.returncode == 2 and "invalid-brightness" in result.stderr, value
    invalid_profile = invoke("energy", "set", "invalid")
    assert invalid_profile.returncode == 2 and "invalid-profile" in invalid_profile.stderr
    assert not Path(env["ANTO_LOCAL_CONFIG_ROOT"]).exists(), "Validation created personal files"
    print(f"services: 16 contracts, {rejected} invalid arities and finite numeric input verified")
