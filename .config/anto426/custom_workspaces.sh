#!/usr/bin/env bash
# Get workspaces info
workspaces=$(hyprctl workspaces -j 2>/dev/null)
first=$(echo "$workspaces" | jq 'map(.id) | min' 2>/dev/null)
last=$(echo "$workspaces" | jq 'map(.id) | max' 2>/dev/null)
active=$(hyprctl activeworkspace -j 2>/dev/null | jq '.id' 2>/dev/null)

# Defaults
first=${first:-1}
last=${last:-1}
active=${active:-1}

# Format with Pango markup
# Active is highlighted in active theme accent color
if [ -f "$HOME/.config/colors/colors.sh" ]; then
    source "$HOME/.config/colors/colors.sh"
fi
accent_color="${ANTO426_ACCENT:-#db81aa}"
active_markup="<span color='$accent_color'><b>$active</b></span>"

if [ "$first" -eq "$last" ]; then
    text="$active_markup"
elif [ "$active" -eq "$first" ]; then
    text="$active_markup • $last"
elif [ "$active" -eq "$last" ]; then
    text="$first • $active_markup"
else
    text="$first • $active_markup • $last"
fi

echo "{\"text\":\"$text\",\"tooltip\":\"Workspaces: Primo ($first) • Attivo ($active) • Ultimo ($last)\"}"
