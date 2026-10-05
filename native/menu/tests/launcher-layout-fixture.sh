#!/usr/bin/env bash
set -euo pipefail

project="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd -P)"
launcher="$project/src/modules/launcher/view.c"
style="$project/assets/components/controls.css.in"
ui="$project/src/core/components.c"

fail() {
    printf 'launcher-layout fixture: %s\n' "$*" >&2
    exit 1
}

# Applications must use the same common geometry as action tiles.  A fourth
# column or launcher-specific CSS would make them smaller again.
rg -Fq 'menu_set_grid_columns(app, 3);' "$launcher" ||
    fail 'la griglia applicazioni non usa tre colonne'
rg -Fq 'gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(app->grid), app->grid_columns);' "$ui" ||
    fail 'il minimo della griglia non segue le tre colonne richieste'
rg -Fq 'gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(app->grid), app->grid_columns);' "$ui" ||
    fail 'il massimo della griglia non e bloccato a tre colonne'
if grep -Eq 'launcher-tile[^,{]*(\{|,)|launcher-tile[[:space:]]+\.' "$style"; then
    fail 'sono ricomparse override geometriche specifiche per le app'
fi

# The launcher passes no subtitle and no badge: the visible card is strictly
# icon + application name, while descriptions may remain search-only data.
awk '
    /menu_add_tile\(app, anto_launcher_app_icon_name\(record->info\), record->title,/ {
        found = 1
        capture = $0
        next
    }
    found && capture !~ /;/ {
        capture = capture " " $0
        if ($0 ~ /;/) exit
    }
    END {
        if (!found || capture !~ /NULL,[[:space:]]+NULL,[[:space:]]+anto_launcher_launch_app/)
            exit 1
    }
' "$launcher" || fail 'le card app contengono descrizione, badge o shortcut'

rg -Fq 'flowboxchild.menu-tile {' "$style" ||
    fail 'manca la geometria condivisa delle card Azioni'
rg -Fq 'anto_ui_tile(' "$project/src/ui/tiles.c" ||
    fail 'le schede non usano la primitiva condivisa'
rg -Fq '"ui-tile"' "$project/../common/src/ui/surfaces.c" ||
    fail 'manca la scheda nella libreria condivisa'

printf 'launcher-layout fixture: PASS\n'
