#!/usr/bin/env bash
set -euo pipefail

cursor_file="$HOME/.icons/default/index.theme"

mkdir -p "$(dirname "$cursor_file")"
cat >"$cursor_file" <<'EOF'
[icon theme]
Inherits=macOS
EOF

sudo install -Dm644 "$cursor_file" /usr/share/icons/default/index.theme
