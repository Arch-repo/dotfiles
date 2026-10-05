#!/usr/bin/env python3
"""Install checksum-pinned OCR, emoji and application theme resources."""
from pathlib import Path
import hashlib
import json
import os
import urllib.request

ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / "resources.lock.json"
DEST = Path(os.environ.get("XDG_DATA_HOME", Path.home() / ".local/share")) / "anto-desktop/tessdata"

def install_app_themes(manifest):
    for name, theme in manifest.get("app_themes", {}).items():
        directory = DEST.parent / name
        directory.mkdir(parents=True, exist_ok=True)
        for item in theme["files"]:
            target = directory / item["filename"]
            if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == item["sha256"]:
                continue
            url = f'https://raw.githubusercontent.com/{theme["repository"]}/{theme["revision"]}/{item["path"]}'
            with urllib.request.urlopen(url, timeout=30) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != item["sha256"]:
                raise RuntimeError(f"Application theme checksum mismatch: {name}/{item['filename']}")
            temporary = target.with_suffix(".download")
            temporary.write_bytes(content)
            temporary.replace(target)

def main():
    manifest = json.loads(LOCK.read_text())
    DEST.mkdir(parents=True, exist_ok=True)
    for item in manifest["ocr"]:
        target = DEST / item["filename"]
        if target.exists() and hashlib.sha256(target.read_bytes()).hexdigest() == item["sha256"]:
            continue
        with urllib.request.urlopen(item["url"], timeout=30) as response:
            content = response.read()
        if hashlib.sha256(content).hexdigest() != item["sha256"]:
            raise RuntimeError(f"Checksum mismatch: {item['filename']}")
        temporary = target.with_suffix(".download")
        temporary.write_bytes(content)
        temporary.replace(target)
    from emoji_catalog import convert
    emoji=manifest["emoji"]
    for key, checksum, filename in (("url", "sha256", "emoji-source.txt"),):
        target=DEST.parent/filename
        if not target.exists() or hashlib.sha256(target.read_bytes()).hexdigest()!=emoji[checksum]:
            with urllib.request.urlopen(emoji[key],timeout=30) as response:content=response.read()
            if hashlib.sha256(content).hexdigest()!=emoji[checksum]:raise RuntimeError("Emoji resource checksum mismatch")
            temporary=target.with_suffix(".download");temporary.write_bytes(content);temporary.replace(target)
    license_content=(ROOT/emoji["license"]).read_bytes()
    if hashlib.sha256(license_content).hexdigest()!=emoji["license_sha256"]:raise RuntimeError("Unicode license checksum mismatch")
    (DEST.parent/"Unicode-LICENSE.txt").write_bytes(license_content)
    catalog=convert((DEST.parent/"emoji-source.txt").read_text())
    (DEST.parent/"emoji.tsv").write_text(catalog)
    install_app_themes(manifest)
    print("OCR, Unicode emoji and application theme resources verified")

if __name__ == "__main__":
    main()
