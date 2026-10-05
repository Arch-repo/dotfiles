#!/usr/bin/env python3
"""Build the pinned compositor renderer against matching Hyprland headers."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent.parent
LOCAL = Path.home() / ".local"

def run(*args, cwd=ROOT):
    subprocess.run([str(arg) for arg in args], cwd=cwd, check=True)

def write_loader(library):
    # Hyprland's plugin directive does not expand '~'. Keep machine paths
    # separate from the shared visual configuration.
    destination = Path.home() / ".config/anto426-local/hypr/glass-loader.conf"
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(".installing")
    temporary.write_text(f"plugin = {library}\n")
    temporary.replace(destination)

def install():
    dependency = json.loads((ROOT / "resources.lock.json").read_text())["glass"]
    flags = subprocess.check_output(["pkg-config", "--cflags", "hyprland"], text=True).split()
    headers = next((Path(flag[2:]) / "src/version.h" for flag in flags
                    if flag.startswith("-I") and (Path(flag[2:]) / "src/version.h").exists()), None)
    if not headers:
        raise RuntimeError("Hyprland development headers are missing")
    revision = re.search(r'GIT_COMMIT_HASH\s+"([0-9a-f]+)"', headers.read_text()).group(1)
    running = json.loads(subprocess.check_output(["Hyprland", "--version-json"], text=True))["commit"]
    if revision != dependency["hyprland_revision"] or running != revision:
        raise RuntimeError("The renderer requires the pinned Hyprland version and matching headers; rebuild after updating its lock")
    source = ROOT / "build/dependencies/hyprglass"
    patch = ROOT / dependency["patch"]
    library = LOCAL / "lib/hyprland/hyprglass.so"
    receipt = LOCAL / "share/anto-desktop/hyprglass-build.json"
    identity = dict(revision=dependency["revision"], hyprland_revision=revision,
                    patch_sha256=hashlib.sha256(patch.read_bytes()).hexdigest())
    if receipt.exists() and library.exists():
        saved = json.loads(receipt.read_text())
        if all(saved.get(key) == value for key, value in identity.items()) and saved.get("sha256") == hashlib.sha256(library.read_bytes()).hexdigest():
            write_loader(library)
            return
    if not (source / ".git").exists():
        source.mkdir(parents=True, exist_ok=True)
        run("git", "init", source)
        run("git", "remote", "add", "origin", dependency["repository"], cwd=source)
        run("git", "fetch", "--depth", "1", "origin", dependency["revision"], cwd=source)
        run("git", "checkout", "--detach", "FETCH_HEAD", cwd=source)
    actual = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    if actual != dependency["revision"]:
        raise RuntimeError("Cached renderer checkout has an unexpected revision")
    applied = subprocess.run(["git", "apply", "--reverse", "--check", str(patch)], cwd=source, capture_output=True)
    if applied.returncode:
        run("git", "apply", "--check", patch, cwd=source)
        run("git", "apply", patch, cwd=source)
    run("make", "clean", cwd=source)
    run("make", "-j", "4", cwd=source)
    library.parent.mkdir(parents=True, exist_ok=True)
    temporary = library.with_suffix(".installing")
    shutil.copy2(source / "hyprglass.so", temporary)
    temporary.replace(library)
    receipt.parent.mkdir(parents=True, exist_ok=True)
    identity["sha256"] = hashlib.sha256(library.read_bytes()).hexdigest()
    receipt.write_text(json.dumps(identity, indent=2) + "\n")
    shutil.copy2(source / "LICENSE", receipt.parent / "hyprglass-LICENSE")
    write_loader(library)

if __name__ == "__main__":
    install()
