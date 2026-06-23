#!/usr/bin/env bash
set -uo pipefail

if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

NOTIFICATIONS_VIEW_MODE="all"

notifications_markup_escape() {
    printf '%s' "$1" |
        sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
}

notifications_start_logger() {
    if ! pgrep -u "${USER:-$(id -un)}" -f "$HOME/.config/anto426/notification_log.sh" >/dev/null 2>&1; then
        nohup "$HOME/.config/anto426/notification_log.sh" </dev/null >/dev/null 2>&1 &
        disown "$!" 2>/dev/null || true
    fi
}

notifications_swaync() {
    local timeout_s="${ANTO426_SWAYNC_TIMEOUT:-1s}"

    timeout "$timeout_s" swaync-client "$@" 2>/dev/null
}

notifications_copy() {
    local label="$1"
    local text="$2"

    if command -v wl-copy >/dev/null 2>&1; then
        printf '%s' "$text" | wl-copy && notify "$label copiato"
    else
        notify "wl-copy non disponibile"
    fi
}

notifications_extract_path() {
    local text="$1"
    local path

    path="$(
        printf '%s\n' "$text" |
            grep -Eo '(/[^[:space:]]+)(png|jpg|jpeg|webp|mkv|mp4|webm|gif|pdf|txt|log)?' |
            sed -n '1p'
    )"

    [[ -n "$path" ]] && printf '%s' "$path"
}

notifications_history_rows() {
    local mode="${1:-all}"
    local limit="${2:-18}"

    [[ -s "$NOTIFICATIONS_FILE" ]] || return 0

    tail -n 100 "$NOTIFICATIONS_FILE" |
        awk -F'\t' -v mode="$mode" -v limit="$limit" '
            function clean(s) {
                gsub(/[\r\n\t]+/, " ", s)
                gsub(/[[:space:]]+/, " ", s)
                sub(/^[[:space:]]+/, "", s)
                sub(/[[:space:]]+$/, "", s)
                return s
            }
            function lower(s) { return tolower(s) }
            function category(app, summary, body, text, app_l) {
                text = lower(app " " summary " " body)
                app_l = lower(app)
                if (text ~ /(urgent|importante|critical|errore|error|failed|fallito|warning|attenzione|deadline|scadenza|low battery|batteria scarica)/) return "urgent"
                if (text ~ /(calendar|calendario|meeting|meet|evento|event|reminder|promemoria)/) return "calendar"
                if (app_l ~ /(mail|gmail|thunderbird|outlook)/ || text ~ /(mail|email|posta)/) return "mail"
                if (app_l ~ /(telegram|whatsapp|signal|discord|slack|teams|element)/ || text ~ /(message|messaggio|reply|risposta|chat)/) return "message"
                if (app_l ~ /(spotify|player|cider|music|vlc)/ || text ~ /(playing|track|song|brano|audio)/) return "media"
                if (text ~ /(download|scaric|completed|complete|finito|terminato)/) return "download"
                if (text ~ /(system|update|pacman|flatpak|network|wifi|bluetooth|battery|power|hyprland|swaync)/) return "system"
                return "info"
            }
            function priority(cat) {
                if (cat == "urgent") return 3
                if (cat == "message" || cat == "mail" || cat == "calendar") return 2
                if (cat == "system" || cat == "download") return 1
                return 0
            }
            function icon(cat) {
                if (cat == "urgent") return "dialog-warning"
                if (cat == "calendar") return "office-calendar"
                if (cat == "mail") return "mail-unread"
                if (cat == "message") return "mail-message-new"
                if (cat == "media") return "multimedia-player"
                if (cat == "download") return "folder-download"
                if (cat == "system") return "preferences-system"
                return "preferences-desktop-notification"
            }
            BEGIN {
                query = ""
                app_filter = ""
                if (index(mode, "search:") == 1) query = lower(substr(mode, 8))
                if (index(mode, "app:") == 1) app_filter = substr(mode, 5)
            }
            NF >= 4 {
                ts = $1
                app = clean($2)
                summary = clean($3)
                body = clean($4)
                if (app == "") app = "App"

                text = app " " summary " " body
                cat = category(app, summary, body)
                prio = priority(cat)
                if (mode == "important" && prio < 2) next
                if (app_filter != "" && app != app_filter) next
                if (query != "" && index(lower(text), query) == 0) next

                preview = summary
                if (body != "") preview = preview " - " body
                label = sprintf("%s  %s · %s", strftime("%H:%M", ts), app, preview)
                if (length(label) > 118) label = substr(label, 1, 115) "..."
                rows[++n] = label "\t" ts "\t" app "\t" summary "\t" body "\t" icon(cat) "\t" cat "\t" prio
            }
            END {
                start = n - limit + 1
                if (start < 1) start = 1
                for (i = n; i >= start; i--) print rows[i]
            }
        '
}

notifications_app_rows() {
    [[ -s "$NOTIFICATIONS_FILE" ]] || return 0

    tail -n 100 "$NOTIFICATIONS_FILE" |
        awk -F'\t' '
            function clean(s) {
                gsub(/[\r\n\t]+/, " ", s)
                gsub(/[[:space:]]+/, " ", s)
                sub(/^[[:space:]]+/, "", s)
                sub(/[[:space:]]+$/, "", s)
                return s
            }
            NF >= 4 {
                app = clean($2)
                if (app == "") app = "App"
                count[app]++
                last[app] = $1
            }
            END {
                for (app in count) {
                    printf "%s (%d, last %s)\t%s\t%d\tpreferences-desktop-notification\n", app, count[app], strftime("%H:%M", last[app]), app, last[app]
                }
            }
        ' |
        sort -t $'\t' -k3,3nr |
        sed -n '1,12p'
}

notifications_smart_message() {
    local active_count="$1"
    local dnd="$2"
    local mode="$3"
    local dnd_label="off"
    local dnd_color="$c_muted"
    [[ "$dnd" == "true" ]] && {
        dnd_label="active"
        dnd_color="$c_red"
    }

    local stats
    stats="$(
        if [[ -s "$NOTIFICATIONS_FILE" ]]; then
            tail -n 100 "$NOTIFICATIONS_FILE" |
                awk -F'\t' '
                    function clean(s) {
                        gsub(/[\r\n\t]+/, " ", s)
                        gsub(/[[:space:]]+/, " ", s)
                        sub(/^[[:space:]]+/, "", s)
                        sub(/[[:space:]]+$/, "", s)
                        return s
                    }
                    function lower(s) { return tolower(s) }
                    function cat(app, summary, body, text) {
                        text = lower(app " " summary " " body)
                        if (text ~ /(urgent|importante|critical|errore|error|failed|fallito|warning|attenzione|deadline|scadenza|low battery|batteria scarica)/) return "urgent"
                        if (text ~ /(calendar|calendario|meeting|meet|evento|event|reminder|promemoria)/) return "calendar"
                        if (text ~ /(mail|gmail|thunderbird|outlook|email|posta)/) return "mail"
                        if (text ~ /(telegram|whatsapp|signal|discord|slack|teams|element|message|messaggio|chat)/) return "message"
                        if (text ~ /(system|update|pacman|flatpak|network|wifi|bluetooth|battery|power|hyprland|swaync)/) return "system"
                        return "other"
                    }
                    NF >= 4 {
                        total++
                        app = clean($2)
                        if (app == "") app = "App"
                        apps[app]++
                        c = cat(app, $3, $4)
                        cats[c]++
                        if (c == "urgent" || c == "calendar" || c == "mail" || c == "message") important++
                        latest = $1
                    }
                    END {
                        top = "none"
                        top_count = 0
                        for (app in apps) if (apps[app] > top_count) { top = app; top_count = apps[app] }
                        printf "%d\t%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s", total, important + 0, top, top_count, cats["urgent"] + 0, cats["message"] + 0, cats["mail"] + 0, cats["calendar"] + 0, latest
                    }
                '
        else
            printf '0\t0\tnone\t0\t0\t0\t0\t0\t'
        fi
    )"

    local total important top_app top_count urgent messages mail calendar latest
    IFS=$'\t' read -r total important top_app top_count urgent messages mail calendar latest <<< "$stats"

    local view_label="$mode"
    case "$mode" in
        all) view_label="Smart inbox" ;;
        important) view_label="Important only" ;;
        app:*) view_label="App: ${mode#app:}" ;;
        search:*) view_label="Search: ${mode#search:}" ;;
    esac

    printf '<b>Active</b>: <span foreground="%s">%s</span>   <b>DND</b>: <span foreground="%s">%s</span>\n' "$c_yellow" "$active_count" "$dnd_color" "$dnd_label"
    printf '<b>View</b>: <span foreground="%s">%s</span>   <b>History</b>: %s\n' "$c_accent" "$(notifications_markup_escape "$view_label")" "${total:-0}"
    printf '<b>Important</b>: <span foreground="%s">%s</span>   <b>Top app</b>: %s (%s)\n' "$c_red" "${important:-0}" "$(notifications_markup_escape "${top_app:-none}")" "${top_count:-0}"
    printf '<span foreground="%s">urgent %s · chat %s · mail %s · calendar %s</span>' "$c_muted" "${urgent:-0}" "${messages:-0}" "${mail:-0}" "${calendar:-0}"
    [[ -n "${latest:-}" ]] && printf '\n<span foreground="%s">last: %s</span>' "$c_muted" "$(date -d "@$latest" '+%d/%m %H:%M' 2>/dev/null || printf '%s' "$latest")"
}

notifications_delete_row() {
    local timestamp="$1"
    local app="$2"
    local summary="$3"
    local body="$4"
    local tmp

    [[ -s "$NOTIFICATIONS_FILE" ]] || return 0
    tmp="$(mktemp)"
    awk -F'\t' -v OFS='\t' \
        -v ts="$timestamp" -v app="$app" -v summary="$summary" -v body="$body" \
        '!(($1 == ts) && ($2 == app) && ($3 == summary) && ($4 == body)) {print}' \
        "$NOTIFICATIONS_FILE" > "$tmp" &&
        mv "$tmp" "$NOTIFICATIONS_FILE"
}

notifications_detail_menu() {
    local row="$1"
    local timestamp app summary body icon category priority detail choice when path folder

    timestamp="$(printf '%s' "$row" | awk -F'\t' '{print $2}')"
    app="$(printf '%s' "$row" | awk -F'\t' '{print $3}')"
    summary="$(printf '%s' "$row" | awk -F'\t' '{print $4}')"
    body="$(printf '%s' "$row" | awk -F'\t' '{print $5}')"
    icon="$(printf '%s' "$row" | awk -F'\t' '{print $6}')"
    category="$(printf '%s' "$row" | awk -F'\t' '{print $7}')"
    priority="$(printf '%s' "$row" | awk -F'\t' '{print $8}')"
    when="$(date -d "@$timestamp" '+%A %d/%m/%Y %H:%M' 2>/dev/null || printf '%s' "$timestamp")"
    path="$(notifications_extract_path "$summary $body")"
    if [[ -n "$path" ]]; then
        if [[ -e "$path" ]]; then
            folder="$(dirname "$path")"
        elif [[ -d "$(dirname "$path")" ]]; then
            folder="$(dirname "$path")"
        fi
    fi

    detail="<b>$(notifications_markup_escape "$app")</b>\n"
    detail="${detail}<span foreground='${c_muted}'>${when} · ${category} · priority ${priority}</span>\n\n"
    detail="${detail}<b>$(notifications_markup_escape "$summary")</b>"
    [[ -n "$body" ]] && detail="${detail}\n$(notifications_markup_escape "$body")"

    while true; do
        choice="$(
            {
                printf 'Show same app\0icon\x1f%s\n' "${icon:-preferences-desktop-notification}"
                [[ -n "$path" && -e "$path" ]] && printf 'Open file\0icon\x1fdocument-open\n'
                [[ -n "$folder" ]] && printf 'Open folder\0icon\x1ffolder-open\n'
                printf 'Copy text\0icon\x1fedit-copy\n'
                printf 'Copy smart summary\0icon\x1fedit-copy\n'
                printf 'Delete from history\0icon\x1fuser-trash\n'
                printf 'Back\0icon\x1fgo-previous\n'
            } | rofi_pick_msg "Notification" "$detail"
        )"

        [[ -z "$choice" ]] && return 0
        case "$choice" in
            "Show same app")
                NOTIFICATIONS_VIEW_MODE="app:$app"
                return 0
                ;;
            "Open file")
                xdg-open "$path" >/dev/null 2>&1 &
                return 0
                ;;
            "Open folder")
                xdg-open "$folder" >/dev/null 2>&1 &
                return 0
                ;;
            "Copy text")
                notifications_copy "Testo notifica" "$summary"$'\n'"$body"
                ;;
            "Copy smart summary")
                notifications_copy "Riepilogo notifica" "[$category/prio $priority] $app: $summary${body:+ - $body}"
                ;;
            "Delete from history")
                notifications_delete_row "$timestamp" "$app" "$summary" "$body"
                notify "Notifica rimossa dalla history"
                return 0
                ;;
            "Back")
                return 0
                ;;
        esac
    done
}

notifications_choose_app() {
    local app_rows choice selected app
    app_rows="$(notifications_app_rows)"
    [[ -n "$app_rows" ]] || {
        notify "Nessuna app nella history"
        return 0
    }

    choice="$(
        while IFS=$'\t' read -r label app _timestamp icon; do
            [[ -n "$app" ]] || continue
            printf '%s\0icon\x1f%s\n' "$label" "${icon:-preferences-desktop-notification}"
        done <<< "$app_rows" |
            rofi_pick "Notification apps"
    )"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$app_rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    app="$(printf '%s' "$selected" | awk -F'\t' '{print $2}')"
    [[ -n "$app" ]] && NOTIFICATIONS_VIEW_MODE="app:$app"
}

notifications_search() {
    local query
    query="$(rofi_input "Search notifications")"
    [[ -n "$query" ]] && NOTIFICATIONS_VIEW_MODE="search:$query"
}

notifications_menu() {
    notifications_start_logger
    mkdir -p "$(dirname "$NOTIFICATIONS_FILE")"
    touch "$NOTIFICATIONS_FILE"

    while true; do
        local count dnd history_rows choice message_card dnd_action dnd_icon view_action
        count="$(notifications_swaync -c || true)"
        [[ "$count" =~ ^[0-9]+$ ]] || count=0
        dnd="$(notifications_swaync -D || true)"
        [[ "$dnd" == "true" || "$dnd" == "false" ]] || dnd=false
        history_rows="$(notifications_history_rows "$NOTIFICATIONS_VIEW_MODE")"
        message_card="$(notifications_smart_message "$count" "$dnd" "$NOTIFICATIONS_VIEW_MODE")"

        local dnd_label dnd_icon
        if [[ "$dnd" == "true" ]]; then
            dnd_label="$(menu_item "󰂚" "Disable Do Not Disturb")"
            dnd_icon="notifications-disabled"
        else
            dnd_label="$(menu_item "󰂛" "Enable Do Not Disturb")"
            dnd_icon="preferences-system-notifications"
        fi

        local view_label view_icon
        if [[ "$NOTIFICATIONS_VIEW_MODE" == "important" ]]; then
            view_label="$(menu_item "󰂚" "Smart inbox")"
            view_icon="mail-inbox"
        else
            view_label="$(menu_item "󰂛" "Important only")"
            view_icon="emblem-important"
        fi

        choice="$(
            {
                printf '%s\0icon\x1f%s\n' "$dnd_label" "$dnd_icon"
                printf '%s\0icon\x1f%s\n' "$view_label" "$view_icon"
                printf '%s\0icon\x1fview-list\n' "$(menu_item "󰈺" "Filter by app")"
                printf '%s\0icon\x1fsystem-search\n' "$(menu_item "󰍉" "Search history")"
                printf '%s\0icon\x1fpreferences-desktop-notification\n' "$(menu_item "󰂚" "Open notification center")"
                printf '%s\0icon\x1fwindow-close\n' "$(menu_item "󰅖" "Close last notification")"
                printf '%s\0icon\x1fedit-clear\n' "$(menu_item "󰅖" "Clear all notifications")"
                printf '%s\0icon\x1fview-refresh\n' "$(menu_item "󰑐" "Refresh")"

                if [[ -n "$history_rows" ]]; then
                    while IFS=$'\t' read -r label _timestamp _app _summary _body icon _category _priority; do
                        [[ -n "$label" ]] || continue
                        printf '%s\0icon\x1f%s\n' "$label" "${icon:-preferences-desktop-notification}"
                    done <<< "$history_rows"
                else
                    printf '%s\0icon\x1finfo\n' "$(menu_item "󰄧" "No notifications in this view")"
                fi

                printf '%s\0icon\x1fgo-previous\n' "$(menu_item "󰌍" "Back")"
            } | rofi_pick_msg "Notifications" "$message_card"
        )"

        [[ -z "$choice" ]] && return 0

        case "$choice" in
            "$(system_text "Disable Do Not Disturb")") notifications_swaync -df >/dev/null || true ;;
            "$(system_text "Enable Do Not Disturb")") notifications_swaync -dn >/dev/null || true ;;
            "$(system_text "Important only")") NOTIFICATIONS_VIEW_MODE="important" ;;
            "$(system_text "Smart inbox")") NOTIFICATIONS_VIEW_MODE="all" ;;
            "$(system_text "Filter by app")") notifications_choose_app ;;
            "$(system_text "Search history")") notifications_search ;;
            "$(system_text "Open notification center")")
                notifications_swaync -t -sw >/dev/null || notifications_swaync -t >/dev/null || true
                return 0
                ;;
            "$(system_text "Close last notification")")
                notifications_swaync --close-latest >/dev/null || true
                [[ -s "$NOTIFICATIONS_FILE" ]] && sed -i '$d' "$NOTIFICATIONS_FILE"
                ;;
            "$(system_text "Clear all notifications")")
                notifications_swaync -C >/dev/null || true
                : > "$NOTIFICATIONS_FILE"
                NOTIFICATIONS_VIEW_MODE="all"
                ;;
            "$(system_text "Refresh")") continue ;;
            "$(system_text "No notifications in this view")") continue ;;
            "$(system_text "Back")")
                back_or_main
                return 0
                ;;
            *)
                local selected
                selected="$(printf '%s\n' "$history_rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
                [[ -n "$selected" ]] && notifications_detail_menu "$selected"
                ;;
        esac
    done
}

notifications_menu
