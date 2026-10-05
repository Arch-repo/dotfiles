#!/usr/bin/env python3
"""Keep primitive construction in the shared library across native surfaces."""
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
constructors = re.compile(r"\bgtk_(?:button|toggle_button|switch|scale|entry|password_entry|"
                          r"search_entry|progress_bar|level_bar|calendar|text_view|picture|"
                          r"label|image|spinner)_new\w*\s*\(")
failures = []
for folder in ("native/menu/src", "native/wallpaper/gallery", "native/osd", "native/widgets"):
    for source in (root / folder).rglob("*"):
        if source.suffix not in (".c", ".cpp"):
            continue
        for number, line in enumerate(source.read_text().splitlines(), 1):
            if constructors.search(line):
                failures.append(f"{source.relative_to(root)}:{number}: primitive built outside library")
for template in (root / "native/menu/assets").rglob("*.css.in"):
    if template.name in ("glass.css.in", "tokens.css.in"):
        continue
    for number, line in enumerate(template.read_text().splitlines(), 1):
        if re.search(r"(?:^|[ ,])(?:button|entry|passwordentry|switch|scale)(?:[: .{]|$)", line):
            failures.append(f"{template.relative_to(root)}:{number}: control styled outside library")
assert not failures, "\n".join(failures)
print("primitives: menu, gallery, OSD and widgets share constructors and control styles")
