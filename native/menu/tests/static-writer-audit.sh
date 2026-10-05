#!/usr/bin/env bash
set -euo pipefail

project="$(cd "$(dirname "$0")/.." && pwd)"
dotfiles="$(cd "$project/../.." && pwd)"
launcher="$dotfiles/support/compat/notes_launcher.sh"
backend="$dotfiles/native/services/src/domains/notes/provider.c"
main="$dotfiles/native/services/src/main.c"
registry="$dotfiles/native/services/src/runtime/service.c"
makefile="$dotfiles/scripts/install.py"
calendar_backend="$dotfiles/native/services/src/domains/calendar/provider.c"
display_backend="$dotfiles/native/services/src/domains/display/provider.c"

fail() {
    printf 'static writer audit: %s\n' "$*" >&2
    exit 1
}

[[ -f "$launcher" ]] || fail "launcher note mancante"
[[ -f "$backend" ]] || fail "backend note mancante"

rg -q 'anto-menu-backend' "$launcher" ||
    fail "il launcher note non delega al backend"
if rg -q '(XDG_CONFIG_HOME|notes[.]env|(^|[^<])>{1,2}[[:space:]]*["$])' \
    "$launcher"; then
    fail "il launcher note contiene di nuovo configurazione o writer"
fi

rg -q 'anto_local_config_(path|write_text|migrate_file)' "$backend" ||
    fail "il backend note non usa local_config"
rg -Fq '&anto_service_notes' "$registry" ||
    fail "il dominio notes non è registrato"
rg -Fq 'g_strcmp0(program, "anto-config")' "$main" ||
    fail "l’alias anto-config non è instradato dal backend"
rg -q 'anto-config' "$makefile" ||
    fail "anto-config non viene installato"

for domain in calendar display; do
    shim="$project/scripts/actions/$domain.sh"
    [[ -x "$shim" ]] || fail "shim $domain mancante o non eseguibile"
    [[ "$(wc -l <"$shim")" -le 12 ]] ||
        fail "lo shim $domain contiene di nuovo logica applicativa"
    rg -q 'exec "\$backend" '"$domain"' "\$@"' "$shim" ||
        fail "lo shim $domain non preserva argv verso il backend C"
    if rg -q '\b(jq|hyprctl|flock|notify-send|mktemp|date)\b|[|][|]?|(^|[^<])>{1,2}' "$shim"; then
        fail "lo shim $domain contiene parsing, pipeline o writer"
    fi
done
[[ -f "$calendar_backend" ]] || fail "backend calendario C mancante"
[[ -f "$display_backend" ]] || fail "backend display C mancante"
rg -Fq '&anto_service_calendar' "$registry" ||
    fail "il dominio calendar non è registrato"
rg -Fq '&anto_service_display' "$registry" ||
    fail "il dominio display non è registrato"
rg -q 'atomic_write_json' "$calendar_backend" ||
    fail "il calendario C ha perso la scrittura atomica"
rg -q 'guarded_mutation' "$dotfiles/native/services/src/domains/display/transaction.c" ||
    fail "il display C ha perso rollback/guardia transazionale"

legacy=(network bluetooth performance session capture record)
for action in "${legacy[@]}"; do
    [[ ! -e "$project/scripts/actions/$action.sh" ]] ||
        fail "adapter shell ripristinato: $action.sh"
done

if rg -n \
    '[.]local/lib/anto-menu/actions/(network|bluetooth|performance|session|capture|record)[.]sh' \
    "$project/src" "$project/include" "$project/assets" "$project/scripts"; then
    fail "riferimento runtime a un adapter shell rimosso"
fi

printf 'static writer audit: ok\n'
