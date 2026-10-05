#!/usr/bin/env bash
set -euo pipefail

pango_escape() {
    printf '%s' "$1" |
        sed -e 's/&/\&amp;/g' \
            -e 's/</\&lt;/g' \
            -e 's/>/\&gt;/g' \
            -e "s/'/\&apos;/g" \
            -e 's/"/\&quot;/g'
}

clients="$(hyprctl clients -j 2>/dev/null || printf '[]')"
filtered="$({
    printf '%s' "$clients"
} | jq -c '[.[] | select(
    (.mapped // true) == true and
    (.hidden // false) == false and
    ((.class // .initialClass // "") |
      test("^(waybar|swaync|swaync-control-center|anto426-osd|com\\.anto426\\.NativeMenu|anto426\\.widget\\..*)$"; "i") | not)
)]' 2>/dev/null || printf '[]')"

count="$(jq 'length' <<<"$filtered")"
tooltip="$(jq -r '
    if length == 0 then "Nessuna finestra aperta"
    else (["Finestre della sessione (" + (length | tostring) + ")", "──────────────────"] +
          map("• " + (.class // "App") + " — " + (.title // "") +
              " [workspace " + ((.workspace.id // "?") | tostring) + "]")) | join("\n")
    end
' <<<"$filtered")"
tooltip="$(pango_escape "$tooltip")"

text="󰘔"
class="empty"
if (( count > 0 )); then
    class="active"
fi

jq -cn --arg text "$text" --arg tooltip "$tooltip" --arg class "$class" \
    '{text: $text, tooltip: $tooltip, class: $class}'
