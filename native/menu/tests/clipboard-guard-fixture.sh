#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_file="$(cd -- "$script_dir/.." && pwd)/src/modules/clipboard/state.c"

rg -F '#define CLIPBOARD_FAILURE_THROTTLE_USEC' "${source_file%/state.c}/internal.h" >/dev/null
rg -F 'if (!view || view->paste_in_flight) return;' "$source_file" >/dev/null
rg -F 'view->paste_in_flight = TRUE;' "$source_file" >/dev/null
rg -F 'g_get_monotonic_time()' "$source_file" >/dev/null
rg -F 'g_strcmp0(view->last_failure_record, entry->id) == 0' \
    "$source_file" >/dev/null
rg -F 'anto_clipboard_snapshot_start(view);' "$source_file" >/dev/null
rg -F 'entry->id = g_strdup(record->key);' "${source_file%/state.c}/view.c" >/dev/null
rg -F 'char *anto_clipboard_record_id(' "$source_file" >/dev/null
rg -F 'if (!key) continue;' "$source_file" >/dev/null
rg -F 'g_bytes_new(paste->record_id, strlen(paste->record_id))' \
    "$source_file" >/dev/null

# The stable numeric ClipHist id must be the decode input.  Reintroducing the
# mutable list preview here would make old rows fail intermittently again.
if rg -F 'entry->record = g_strdup(record->record);' "$source_file" >/dev/null; then
    printf 'clipboard guard fixture: mutable preview used as decode input\n' >&2
    exit 1
fi

guard_line="$(rg -n -F 'if (!view || view->paste_in_flight) return;' \
    "$source_file" | cut -d: -f1)"
spawn_line="$(rg -n -F 'GSubprocess *decode = g_subprocess_newv(' \
    "$source_file" | cut -d: -f1)"
if [[ -z "$guard_line" || -z "$spawn_line" || "$guard_line" -ge "$spawn_line" ]]; then
    printf 'clipboard guard fixture: in-flight guard must precede decode spawn\n' >&2
    exit 1
fi

printf 'clipboard guard fixture: ok\n'
