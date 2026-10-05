#!/usr/bin/env bash
set -eu
operation="${2:?operazione mancante}"
if [[ "$operation" == snapshot ]]; then
    if [[ -e "$ANTO_BT_UI_SNAPSHOT.fail" ]]; then
        printf 'ERROR\tsnapshot\tServizio temporaneamente non disponibile\n' >&2
        exit 1
    fi
    cat "$ANTO_BT_UI_SNAPSHOT"
    exit 0
fi
if [[ "$operation" == audio-profiles ]]; then
    exit 0
fi
printf '%s\n' "$operation" >>"$ANTO_BT_UI_SNAPSHOT.actions"
sleep 0.15
if [[ -e "$ANTO_BT_UI_SNAPSHOT.fail-action" ]]; then
    printf 'Operazione rifiutata dal dispositivo\n' >&2
    exit 1
fi
printf 'OK\t%s\n' "$operation"
