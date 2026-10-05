#!/bin/sh
set -eu

# Compatibility protocol only.  Monitor validation, rollback, profiles and
# atomic persistence live in src/backend/display.c.
backend="${ANTO_MENU_BACKEND:-$HOME/.local/libexec/anto-menu/anto-menu-backend}"
if [ ! -x "$backend" ]; then
    backend="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/native-menu/build/bin/anto-menu-backend"
fi
exec "$backend" display "$@"
