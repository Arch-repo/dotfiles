#!/bin/sh
set -eu

backend="${ANTO_MENU_BACKEND:-$HOME/.local/libexec/anto-menu/anto-menu-backend}"
exec "$backend" notes open
