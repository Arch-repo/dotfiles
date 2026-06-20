#!/usr/bin/env bash
set -uo pipefail

# Source shared utils if not already loaded
if [[ -z "${ANTO426_UTILS_LOADED:-}" ]]; then
    source "$(dirname "${BASH_SOURCE[0]}")/utils.sh"
fi

calendar_require_jq() {
    command -v jq >/dev/null 2>&1 || {
        notify "jq non disponibile: calendario disattivato"
        return 1
    }
}

ensure_events_file() {
    calendar_require_jq || return 1
    mkdir -p "$(dirname "$EVENTS_FILE")"
    if [[ ! -s "$EVENTS_FILE" ]] || ! jq empty "$EVENTS_FILE" >/dev/null 2>&1; then
        printf '[]\n' > "$EVENTS_FILE"
    fi
    if [[ ! -s "$GCAL_EVENTS_FILE" ]] || ! jq empty "$GCAL_EVENTS_FILE" >/dev/null 2>&1; then
        printf '[]\n' > "$GCAL_EVENTS_FILE"
    fi
}

calendar_events_for_date() {
    local date="$1"
    ensure_events_file
    jq -s -r --arg date "$date" '
        def clean: tostring | gsub("[\t\r\n]+"; " ");
        ((.[0] // []) + (.[1] // [])) |
        [.[] | select(.date == $date)] |
        sort_by((if (.all_day // false) then "00:00" else (.start // "00:00") end), (.title // ""))[] |
        [
            ((.start // "") | clean),
            ((.end // "") | clean),
            ((.title // "Evento") | clean),
            ((.description // "") | clean),
            ((.id // "") | clean),
            ((.source // "locale") | clean),
            ((.url // "") | clean),
            ((.all_day // false) | tostring)
        ] | @tsv
    ' "$EVENTS_FILE" "$GCAL_EVENTS_FILE"
}

calendar_events_between() {
    local start_date="$1"
    local end_date="$2"
    ensure_events_file
    jq -s -r --arg start "$start_date" --arg end "$end_date" '
        def clean: tostring | gsub("[\t\r\n]+"; " ");
        ((.[0] // []) + (.[1] // [])) |
        [.[] | select(.date >= $start and .date <= $end)] |
        sort_by(.date, (if (.all_day // false) then "00:00" else (.start // "00:00") end), (.title // ""))[] |
        [
            ((.date // "") | clean),
            ((.start // "") | clean),
            ((.end // "") | clean),
            ((.title // "Evento") | clean),
            ((.description // "") | clean),
            ((.id // "") | clean),
            ((.source // "locale") | clean),
            ((.url // "") | clean),
            ((.all_day // false) | tostring)
        ] | @tsv
    ' "$EVENTS_FILE" "$GCAL_EVENTS_FILE"
}

calendar_rows_for_date() {
    local date="$1"
    calendar_events_for_date "$date" |
        awk -F'\t' -v date="$date" '
            NF {
                source_icon = ($6 == "google") ? "󰊭" : "󰃭"
                when = ($8 == "true" || $1 == "") ? "Tutto il giorno" : $1 (($2 != "") ? "-" $2 : "")
                desc = ($4 == "") ? "" : "  · " $4
                label = sprintf("%s %s  %s%s", source_icon, when, $3, desc)
                if (length(label) > 118) label = substr(label, 1, 115) "..."
                printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", label, $5, $6, date, $1, $2, $3, $4, $7, $8
            }
        '
}

calendar_upcoming_rows() {
    local today end_date
    today="$(date +%F)"
    end_date="$(date -d "$today +30 days" +%F)"
    calendar_events_between "$today" "$end_date" |
        awk -F'\t' '
            NF {
                source_icon = ($7 == "google") ? "󰊭" : "󰃭"
                when = ($9 == "true" || $2 == "") ? "Tutto il giorno" : $2 (($3 != "") ? "-" $3 : "")
                desc = ($5 == "") ? "" : "  · " $5
                label = sprintf("%s %s  %s  %s%s", source_icon, $1, when, $4, desc)
                if (length(label) > 118) label = substr(label, 1, 115) "..."
                printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n", label, $6, $7, $1, $2, $3, $4, $5, $8, $9
            }
        '
}

calendar_delete_event_by_id() {
    local id="$1"
    local tmp
    [[ -n "$id" ]] || return 0
    ensure_events_file
    tmp="$(mktemp)"
    jq --arg id "$id" 'map(select(.id != $id))' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"
    notify "Evento locale eliminato"
}

calendar_sync_status() {
    if [[ -s "$GCAL_EVENTS_FILE" ]]; then
        printf 'Google: aggiornato %s' "$(date -r "$GCAL_EVENTS_FILE" '+%d/%m %H:%M' 2>/dev/null)"
    else
        printf 'Google: non sincronizzato'
    fi
}

calendar_sync_google() {
    if [[ ! -x "$REMOTE_SYNC_SCRIPT" ]]; then
        notify "Script sync mancante"
        return 1
    fi

    "$REMOTE_SYNC_SCRIPT" calendar
}

calendar_event_detail_menu() {
    local row="$1"
    local label id source event_date start end title description url all_day source_label when date_label detail choice

    label="$(printf '%s' "$row" | awk -F'\t' '{print $1}')"
    id="$(printf '%s' "$row" | awk -F'\t' '{print $2}')"
    source="$(printf '%s' "$row" | awk -F'\t' '{print $3}')"
    event_date="$(printf '%s' "$row" | awk -F'\t' '{print $4}')"
    start="$(printf '%s' "$row" | awk -F'\t' '{print $5}')"
    end="$(printf '%s' "$row" | awk -F'\t' '{print $6}')"
    title="$(printf '%s' "$row" | awk -F'\t' '{print $7}')"
    description="$(printf '%s' "$row" | awk -F'\t' '{print $8}')"
    url="$(printf '%s' "$row" | awk -F'\t' '{print $9}')"
    all_day="$(printf '%s' "$row" | awk -F'\t' '{print $10}')"

    source_label="Local"
    [[ "$source" == "google" ]] && source_label="Google Calendar"
    date_label="$(date -d "$event_date" '+%a %d/%m/%Y' 2>/dev/null || printf '%s' "$event_date")"
    if [[ "$all_day" == "true" || -z "$start" ]]; then
        when="$date_label, all-day"
    else
        when="$date_label, $start${end:+-$end}"
    fi

    detail="Source: $source_label\nWhen: $when\n\n$title"
    [[ -n "$description" ]] && detail="$detail\n$description"

    choice="$(
        {
            printf '%s\n' "󰅍  Copy Details"
            [[ -n "$url" ]] && printf '%s\n' "󰖟  Open Event"
            [[ "$source" != "google" ]] && printf '%s\n' "󰆴  Delete Local Event"
            printf '%s\n' "󰌍  Back"
        } | rofi_pick_msg "${label%%  *}" "$detail" "$THEME_CALENDAR"
    )"

    case "$choice" in
        "  ──"*) return 0 ;;
        *"Copy Details" | *"Copia dettagli"*)
            if command -v wl-copy >/dev/null 2>&1; then
                printf '%s\n%s\n%s\n' "$title" "$when" "$description" | wl-copy || true
            else
                notify "wl-copy non disponibile"
            fi
            ;;
        *"Open Event" | *"Apri evento"*)
            xdg-open "$url" >/dev/null 2>&1 &
            ;;
        *"Delete Local Event" | *"Elimina evento locale"*)
            calendar_delete_event_by_id "$id"
            ;;
    esac
}

calendar_add_event() {
    local title event_date start_time end_time description normalized id tmp

    title="$(rofi_input "Event Title")"
    [[ -n "$title" ]] || return 0

    event_date="$(rofi_input "Event Date (YYYY-MM-DD)" "$(date +%F)")"
    [[ -n "$event_date" ]] || return 0
    normalized="$(date -d "$event_date" +%F 2>/dev/null)" || {
        notify "Invalid Date"
        return 1
    }

    start_time="$(rofi_input "Start Time (HH:MM)" "$(date +%H:%M)")"
    [[ "$start_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]] || {
        notify "Invalid Start Time"
        return 1
    }

    end_time="$(rofi_input "Optional End Time (HH:MM)")"
    if [[ -n "$end_time" && ! "$end_time" =~ ^([01]?[0-9]|2[0-3]):[0-5][0-9]$ ]]; then
        notify "Invalid End Time"
        return 1
    fi

    description="$(rofi_input "Optional Description")"
    id="$(date +%s%N)"
    ensure_events_file
    tmp="$(mktemp)"

    jq \
        --arg id "$id" \
        --arg title "$title" \
        --arg date "$normalized" \
        --arg start "$start_time" \
        --arg end "$end_time" \
        --arg description "$description" \
        '. += [{
            id: $id,
            title: $title,
            date: $date,
            start: $start,
            end: $end,
            description: $description
        }]' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"

    notify "Event Added: $title"
}

calendar_show_date() {
    local date="$1"
    local rows labels choice selected

    rows="$(calendar_rows_for_date "$date")"
    if [[ -z "$rows" ]]; then
        printf '%s\n' "󰌍  Back" | rofi_pick_msg "$date" "No events for this date" "$THEME_CALENDAR" >/dev/null
        return 0
    fi

    labels="$(printf '%s\n' "$rows" | awk -F'\t' '{print $1}')"
    choice="$(printf '%s\n' "$labels" | rofi_pick "$date" "$THEME_CALENDAR")"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    [[ -n "$selected" ]] && calendar_event_detail_menu "$selected"
}

calendar_show_upcoming() {
    local rows labels choice selected

    rows="$(calendar_upcoming_rows)"
    if [[ -z "$rows" ]]; then
        printf '%s\n' "󰌍  Back" | rofi_pick_msg "Upcoming Events" "No events in the next 30 days" "$THEME_CALENDAR" >/dev/null
        return 0
    fi

    labels="$(printf '%s\n' "$rows" | awk -F'\t' '{print $1}')"
    choice="$(printf '%s\n' "$labels" | rofi_pick "Upcoming 30 days" "$THEME_CALENDAR")"
    [[ -z "$choice" ]] && return 0

    selected="$(printf '%s\n' "$rows" | awk -F'\t' -v chosen="$choice" '$1 == chosen {print; exit}')"
    [[ -n "$selected" ]] && calendar_event_detail_menu "$selected"
}

calendar_delete_event() {
    local item id tmp
    ensure_events_file
    item="$(
        jq -r 'sort_by(.date, .start)[] | "\(.date) \(.start)  \(.title)\t\(.id)"' "$EVENTS_FILE" |
            rofi_pick "Delete Event"
    )"
    id="$(printf '%s' "$item" | awk -F'\t' '{print $2}')"
    [[ -n "$id" ]] || return 0

    tmp="$(mktemp)"
    jq --arg id "$id" 'map(select(.id != $id))' "$EVENTS_FILE" > "$tmp" &&
        mv "$tmp" "$EVENTS_FILE"
    notify "Local event deleted"
}


# Global dynamic colors loaded dynamically from system configuration
c_accent="$(get_color accent "#8cb8e4")"
c_muted="$(get_color muted "#b9c4d2")"
c_yellow="$(get_color yellow "#f9e2af")"
c_green="$(get_color green "#a6e3a1")"
c_red="$(get_color red "#f38ba8")"
c_cyan="$(get_color cyan "#89dceb")"

calendar_month_message() {
    local today month_view events
    today="$(date +%F)"
    
    # Load dynamic theme colors
    local c_accent c_muted c_yellow
    c_accent="$(get_color accent "#8cb8e4")"
    c_muted="$(get_color muted "#b9c4d2")"
    c_yellow="$(get_color yellow "#f9e2af")"

    # Get calendar text
    local day_num
    day_num=$(date +%e | tr -d ' ')
    
    local cal_raw cal_header cal_weekdays cal_body
    cal_raw="$(cal -m 2>/dev/null || ncal -M 2>/dev/null || date '+%B %Y')"
    cal_header="$(echo "$cal_raw" | sed -n '1p')"
    cal_weekdays="$(echo "$cal_raw" | sed -n '2p')"
    cal_body="$(echo "$cal_raw" | tail -n +3)"
    
    # Highlight today's date in cal_body, escaping the day boundaries correctly
    cal_body="$(echo "$cal_body" | sed "s/\b${day_num}\b/<b><span foreground='${c_accent}'>${day_num}<\/span><\/b>/")"
    
    # Format headers cleanly
    month_view="<b><span foreground='${c_accent}'>${cal_header}</span></b>\n<span foreground='${c_muted}'>${cal_weekdays}</span>\n${cal_body}"

    # Get events for today
    events="$(
        calendar_events_for_date "$today" |
            awk -F'\t' '
                NF {
                    time_str = ($8 == "true" || $1 == "") ? "All day" : $1
                    print time_str "\t" $3
                }
            ' |
            sed -n '1,5p'
    )"
    
    local events_formatted
    if [[ -z "$events" ]]; then
        events_formatted="<span foreground='${c_muted}'><i>No appointments</i></span>"
    else
        events_formatted=""
        while IFS=$'\t' read -r ev_time ev_title; do
            if [[ -n "$ev_title" ]]; then
                # Clean event rendering: "12:00  Meeting Name"
                if [[ "$ev_time" == "All day" ]]; then
                    events_formatted="${events_formatted}<span foreground='${c_muted}'>all-day</span>  ${ev_title}\n"
                else
                    events_formatted="${events_formatted}<span foreground='${c_yellow}'>${ev_time}</span>  ${ev_title}\n"
                fi
            fi
        done <<< "$events"
    fi
    
    # Reassemble a beautifully clean, minimalist card
    printf '%b\n\n<span foreground="%s">%s</span>\n\n<b><span foreground="%s">TODAY'\''S APPOINTMENTS</span></b>\n%b' \
        "$month_view" \
        "$c_muted" \
        "$(calendar_sync_status)" \
        "$c_accent" \
        "$events_formatted"
}

calendar_menu() {
    if ! calendar_require_jq; then
        printf 'Back\0icon\x1fgo-previous\n' |
            rofi_pick_msg "Calendar" "jq non disponibile" "$THEME_CALENDAR" >/dev/null
        back_or_main
        return 0
    fi

    while true; do
        local today month choice picked_date message
        today="$(date +%F)"
        month="$(date '+%B %Y')"
        message="$(calendar_month_message)"

        choice="$(
            {
                printf "Today's events\0icon\x1foffice-calendar\n"
                printf "Next 30 days\0icon\x1fappointment-new\n"
                printf "Choose date\0icon\x1fgo-jump\n"
                printf "Add local event\0icon\x1flist-add\n"
                printf "Delete local event\0icon\x1flist-remove\n"
                printf "Sync Google Calendar\0icon\x1fview-refresh\n"
                printf "Open Google Calendar\0icon\x1fweb-browser\n"
                printf "Config sync\0icon\x1fpreferences-system\n"
                printf "Back\0icon\x1fgo-previous\n"
            } | rofi_pick_msg "$month" "$message" "$THEME_CALENDAR"
        )"

        [[ -z "$choice" ]] && return 0
        local clean_choice="$choice"

        case "$clean_choice" in
            "Sync Google Calendar") calendar_sync_google ;;
            "Add local event") calendar_add_event ;;
            "Today's events") calendar_show_date "$today" ;;
            "Next 30 days") calendar_show_upcoming ;;
            "Choose date")
                picked_date="$(rofi_input "Date (YYYY-MM-DD)" "$today")"
                if [[ -n "$picked_date" ]]; then
                    if picked_date="$(date -d "$picked_date" +%F 2>/dev/null)"; then
                        calendar_show_date "$picked_date"
                    else
                        notify "Data non valida"
                    fi
                fi
                ;;
            "Delete local event") calendar_delete_event ;;
            "Open Google Calendar") xdg-open "https://calendar.google.com/calendar/u/0/r" >/dev/null 2>&1 & ;;
            "Config sync") run_script_or_notify "Config sync" "$REMOTE_SYNC_SCRIPT" config ;;
            "Back")
                back_or_main
                return 0
                ;;
            *)
                return 0
                ;;
        esac
    done
}



calendar_menu
