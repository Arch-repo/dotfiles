#!/bin/sh
core="$HOME/.local/libexec/anto426/anto-widgets"
case "${1:-toggle}" in
    menu|arrange) exec "$HOME/.local/bin/anto-menu" widgets ;;
    *) exec "$core" "$@" ;;
esac
