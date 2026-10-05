#!/usr/bin/env python3
"""Generate the registry from one WIDGET_DECLARE per widget source file."""
from pathlib import Path
import re
import sys

source, target = map(Path, sys.argv[1:])
entries = []
for path in sorted(source.glob('*.c')):
    names = re.findall(r'\bWIDGET_DECLARE\s*\(\s*([a-z][a-z0-9_]*)\s*,', path.read_text())
    assert len(names) <= 1, f'{path}: one declaration per file'
    entries.extend(names)
assert entries and len(entries) <= 64 and len(set(entries)) == len(entries), 'Invalid widget registry'
text = '#include "widgets.h"\n/* Generated; add a declaration in the widget source. */\n'
text += ''.join(f'extern const WidgetDefinition widget_definition_{name};\n' for name in entries)
text += 'const WidgetDefinition *const widget_definitions[] = {\n'
text += ''.join(f'  &widget_definition_{name},\n' for name in entries) + '};\n'
text += 'const guint widget_definition_count = G_N_ELEMENTS(widget_definitions);\n'
if not target.exists() or target.read_text() != text:
    target.write_text(text)
