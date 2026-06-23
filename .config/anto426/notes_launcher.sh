#!/usr/bin/env bash
set -uo pipefail

config_file="${XDG_CONFIG_HOME:-$HOME/.config}/anto426/notes.env"
notes_dir="${ANTO426_NOTES_DIR:-$HOME/Documents/Notes}"
default_note="$notes_dir/inbox.md"

mkdir -p "$(dirname "$config_file")" "$notes_dir"

if [[ ! -f "$config_file" ]]; then
    cat >"$config_file" <<EOF
# Command opened by the notes key.
# You can replace this with obsidian, code, sublime_text, nvim, etc.
export ANTO426_NOTES_COMMAND='gnome-text-editor "$HOME/Documents/Notes/inbox.md"'
EOF
fi

# shellcheck disable=SC1090
source "$config_file"

if [[ ! -f "$default_note" ]]; then
    printf '# Inbox\n\n' >"$default_note"
fi

command_to_run="${ANTO426_NOTES_COMMAND:-}"
if [[ -z "$command_to_run" ]]; then
    printf -v quoted_note '%q' "$default_note"
    command_to_run="gnome-text-editor $quoted_note"
fi

setsid bash -lc "$command_to_run" >/dev/null 2>&1 &
