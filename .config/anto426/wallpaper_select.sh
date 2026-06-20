#!/usr/bin/env bash
set -uo pipefail

wallpapers_dir="${ANTO426_WALLPAPERS_DIR:-$HOME/Pictures/Wallpapers}"
theme="$HOME/.config/rofi/control_menu.rasi"
apply_script="$HOME/.config/anto426/wallpaper_apply.sh"
depth="${ANTO426_WALLPAPER_DEPTH:-3}"
quickpaper_config="${ANTO426_HYPRQUICKPAPER_CONFIG:-$HOME/.config/quickshell/hyprquickpaper}"
quickpaper_cache="${ANTO426_HYPRQUICKPAPER_CACHE:-$HOME/.cache/quickshell/hyprquickpaper/thumbs}"
selector="${ANTO426_WALLPAPER_SELECTOR:-auto}"
selector_top_margin="${ANTO426_WALLPAPER_SELECTOR_TOP_MARGIN:-52}"
runtime_dir="${XDG_RUNTIME_DIR:-/tmp}"
selector_lock_dir="$runtime_dir/anto426-wallpaper-select.lock"
selector_lock_held=0

release_selector_lock() {
    (( selector_lock_held )) || return 0
    rm -f "$selector_lock_dir/pid" 2>/dev/null || true
    rmdir "$selector_lock_dir" 2>/dev/null || true
    selector_lock_held=0
}

selector_owner_running() {
    local pid="$1"
    local cmdline

    [[ "$pid" =~ ^[0-9]+$ ]] || return 1
    kill -0 "$pid" 2>/dev/null || return 1
    [[ -r "/proc/$pid/cmdline" ]] || return 0

    cmdline="$(tr '\0' ' ' <"/proc/$pid/cmdline" 2>/dev/null || true)"
    [[ "$cmdline" == *"wallpaper_select.sh"* || "$cmdline" == *"quickshell"*"hyprquickpaper"* ]]
}

acquire_selector_lock() {
    local i owner

    for ((i = 0; i < 8; i++)); do
        if mkdir "$selector_lock_dir" 2>/dev/null; then
            selector_lock_held=1
            printf '%s\n' "$$" >"$selector_lock_dir/pid"
            trap release_selector_lock EXIT
            trap 'release_selector_lock; exit 130' INT TERM
            return 0
        fi

        owner="$(cat "$selector_lock_dir/pid" 2>/dev/null || true)"
        selector_owner_running "$owner" && return 1
        rm -rf "$selector_lock_dir" 2>/dev/null || true
        sleep 0.03
    done

    return 1
}

acquire_selector_lock || exit 0

if pgrep -x rofi >/dev/null; then
    pkill -x rofi
fi

notify() {
    notify-send "Wallpaper" "$*" 2>/dev/null || true
}

go_back() {
    if [[ "${ANTO426_MENU_PARENT:-}" == "control" ]]; then
        release_selector_lock
        exec "$HOME/.config/anto426/control_menu.sh" main
    fi
    exit 0
}

wallpaper_files() {
    [[ -d "$wallpapers_dir" ]] || return 0
    find "$wallpapers_dir" -maxdepth "$depth" -type f \
        \( -iname "*.png" -o -iname "*.jpg" -o -iname "*.jpeg" -o -iname "*.webp" \
           -o -iname "*.mp4" -o -iname "*.webm" -o -iname "*.mkv" -o -iname "*.mov" -o -iname "*.gif" \) |
        sort -f
}

is_video_path() {
    case "${1,,}" in
        *.mp4 | *.webm | *.mkv | *.mov) return 0 ;;
        *) return 1 ;;
    esac
}

current_wallpaper() {
    awww query 2>/dev/null | awk -F'image: ' '/image:/ {print $2; exit}'
}

open_wallpaper_dir() {
    mkdir -p "$wallpapers_dir"
    for opener in nemo dolphin thunar nautilus xdg-open; do
        if command -v "$opener" >/dev/null 2>&1; then
            "$opener" "$wallpapers_dir" >/dev/null 2>&1 &
            return 0
        fi
    done
}

canonical_path() {
    local path="$1"
    [[ -n "$path" ]] || return 1
    readlink -f "$path" 2>/dev/null || printf '%s' "$path"
}

json_escape() {
    printf '%s' "$1" |
        sed -e 's/\\/\\\\/g' -e 's/"/\\"/g'
}

theme_color() {
    local name="$1"
    local fallback="$2"

    awk -v key="$name" -v fallback="$fallback" '
        $1 == "@define-color" && $2 == key {
            value = $0
            sub(/^[[:space:]]*@define-color[[:space:]]+[^[:space:]]+[[:space:]]+/, "", value)
            sub(/[[:space:]]*;[[:space:]]*$/, "", value)
            print value
            found = 1
            exit
        }
        END {
            if (!found) print fallback
        }
    ' "$HOME/.config/colors/colors.css" 2>/dev/null
}

theme_accent() {
    theme_color accent "#8cb8e4"
}

theme_qml_alpha_color() {
    local name="$1"
    local fallback="$2"
    local alpha="$3"
    local color
    local compact

    color="$(theme_color "$name" "")"
    compact="$(printf '%s' "$color" | tr -d '[:space:]')"

    if [[ "$compact" =~ ^rgba\(([0-9]+),([0-9]+),([0-9]+),([0-9.]+)\)$ ]]; then
        awk \
            -v r="${BASH_REMATCH[1]}" \
            -v g="${BASH_REMATCH[2]}" \
            -v b="${BASH_REMATCH[3]}" \
            -v a="${BASH_REMATCH[4]}" '
            function clamp_byte(v) {
                v = int(v + 0.5)
                if (v < 0) return 0
                if (v > 255) return 255
                return v
            }
            BEGIN {
                if (a > 1) a = a / 100
                if (a < 0) a = 0
                if (a > 1) a = 1
                printf "#%02x%02x%02x%02x\n", int(a * 255 + 0.5), clamp_byte(r), clamp_byte(g), clamp_byte(b)
            }
        '
        return 0
    fi

    if [[ "$compact" =~ ^#[[:xdigit:]]{8}$ ]]; then
        printf '%s\n' "$compact"
        return 0
    fi

    if [[ "$compact" =~ ^#[[:xdigit:]]{6}$ ]]; then
        printf '#%s%s\n' "$alpha" "${compact#\#}"
        return 0
    fi

    compact="$(printf '%s' "$fallback" | tr -d '[:space:]')"
    if [[ "$compact" =~ ^#[[:xdigit:]]{6}$ ]]; then
        printf '#%s%s\n' "$alpha" "${compact#\#}"
    else
        printf '#%s1e1e2e\n' "$alpha"
    fi
}

write_hyprquickpaper_shell() {
    cat >"$quickpaper_config/shell.qml" <<'EOF_QML'
import Quickshell
import Quickshell.Io
import QtQuick
import Qt.labs.folderlistmodel
import Quickshell.Wayland

PanelWindow {
    id: main
    implicitHeight: Screen.height
    implicitWidth: Screen.width
    color: "transparent"
    property int speed: 180
    property int stripHeight: Math.min(500, Math.max(320, Math.round(Screen.height * 0.52)))
    property int reservedTop: Math.max(0, configs.top_margin)

    anchors {
        left: true
        right: true
        top: true
        bottom: true
    }

    aboveWindows: true
    exclusionMode: "Ignore"
    exclusiveZone: 0

    WlrLayershell.layer: WlrLayer.Top
    WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
    WlrLayershell.namespace: "hyprquickpaper"

    Component.onCompleted: {
        Quickshell.execDetached(["bash", Quickshell.shellPath("cache.sh"), Quickshell.shellDir])
        list.forceActiveFocus()
    }

    FileView {
        path: Quickshell.shellPath("config.json")
        watchChanges: true
        onFileChanged: reload()

        JsonAdapter {
            id: configs
            property string wallpaper_path
            property string cache_path
            property int number_of_pictures
            property int top_margin
            property string border_color
            property string background_color
        }
    }

    FolderListModel {
        id: folderModel
        folder: "file://" + configs.wallpaper_path
        showDirs: false
        nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.gif", "*.mp4", "*.webm", "*.mkv", "*.mov", "*.MP4", "*.WEBM", "*.MKV", "*.MOV"]
        sortField: FolderListModel.Name
    }

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: main.reservedTop
        color: configs.background_color && configs.background_color.length > 0 ? configs.background_color : "#471e1e2e"
    }

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: Math.round(main.reservedTop / 2)
        anchors.leftMargin: Math.max(0, Screen.width * 0.07)
        anchors.rightMargin: Math.max(0, Screen.width * 0.07)
        height: main.stripHeight
        focus: true

        model: folderModel
        orientation: ListView.Horizontal
        spacing: 8
        clip: true
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        flickDeceleration: 3500
        maximumFlickVelocity: 6000
        cacheBuffer: Math.max(0, width * 2)
        currentIndex: selectedIndex
        highlightRangeMode: ListView.ApplyRange
        preferredHighlightBegin: width * 0.5 - tileWidth * 0.5
        preferredHighlightEnd: width * 0.5 + tileWidth * 0.5

        property int selectedIndex: 0
        property real tileWidth: Math.max(130, width / Math.max(1, configs.number_of_pictures) - spacing)

        function clampIndex(i) {
            return Math.max(0, Math.min(i, count - 1))
        }

        function centerIndex(i) {
            selectedIndex = clampIndex(i)
            positionViewAtIndex(selectedIndex, ListView.Center)
        }

        function activateCurrent() {
            if (count <= 0)
                return
            const path = folderModel.get(selectedIndex, "filePath")
            Quickshell.execDetached(["bash", Quickshell.shellPath("commands.sh"), path])
            Qt.quit()
        }

        function indexNearCenter() {
            if (count <= 0)
                return 0
            const step = tileWidth + spacing
            return clampIndex(Math.round((contentX + width * 0.5 - tileWidth * 0.5) / step))
        }

        function isLiveFile(name) {
            const lower = String(name).toLowerCase()
            return lower.endsWith(".mp4") || lower.endsWith(".webm") || lower.endsWith(".mkv") || lower.endsWith(".mov")
        }

        function cachedPreview(name) {
            return "file://" + configs.cache_path + name + ".png"
        }

        function previewSource(name) {
            return cachedPreview(name)
        }

        Behavior on contentX {
            NumberAnimation {
                duration: main.speed
                easing.type: Easing.OutCubic
            }
        }

        onMovementEnded: selectedIndex = indexNearCenter()
        onFlickEnded: selectedIndex = indexNearCenter()

        delegate: Item {
            id: card
            property bool active: index === list.selectedIndex
            property bool triedOriginalFallback: false
            property bool hadPreview: false
            property int previewRetries: 0
            width: list.tileWidth
            height: list.height
            z: active ? 10 : 0
            scale: active ? 1.12 : 0.98
            transformOrigin: Item.Center

            Behavior on scale {
                NumberAnimation {
                    duration: 140
                    easing.type: Easing.OutCubic
                }
            }

            Image {
                id: img
                z: 1
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                cache: true
                smooth: true
                source: list.previewSource(fileName)
                transform: Shear { xFactor: -0.25 }

                Timer {
                    id: retryTimer
                    interval: 180
                    repeat: false
                    onTriggered: {
                        if (card.previewRetries >= 10)
                            return
                        card.previewRetries += 1
                        const oldSource = img.source
                        img.source = ""
                        img.source = oldSource
                    }
                }

                onSourceChanged: {
                    if (source == list.previewSource(fileName)) {
                        card.hadPreview = false
                        card.triedOriginalFallback = false
                        card.previewRetries = 0
                    }
                }

                onStatusChanged: {
                    if (status === Image.Ready)
                        card.hadPreview = true

                    if (status !== Image.Error)
                        return

                    if (card.previewRetries < 2) {
                        retryTimer.start()
                        return
                    }

                    if (!list.isLiveFile(fileName) && !card.triedOriginalFallback) {
                        card.triedOriginalFallback = true
                        source = fileUrl
                        return
                    }

                    if (card.previewRetries < 10)
                        retryTimer.start()
                }
            }

            Text {
                id: alt
                z: 2
                visible: !card.hadPreview && img.status !== Image.Ready
                text: img.status === Image.Error && card.previewRetries >= 10 ? "No preview" : "Preview"
                color: configs.border_color
                anchors.centerIn: parent
                font.pixelSize: 16
                transform: Shear { xFactor: -0.25 }
            }

            Rectangle {
                z: 10
                anchors.fill: parent
                color: "transparent"
                visible: card.active
                border.width: 5
                border.color: configs.border_color
                transform: Shear { xFactor: -0.25 }
            }

            Rectangle {
                z: 11
                visible: list.isLiveFile(fileName)
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 18
                width: 74
                height: 28
                radius: 14
                color: "#000000aa"
                border.width: 1
                border.color: configs.border_color

                Text {
                    anchors.centerIn: parent
                    text: "LIVE"
                    color: "white"
                    font.pixelSize: 12
                    font.bold: true
                }
            }
        }

        Keys.onPressed: function(event) {
            const step = 1
            const big = Math.max(1, configs.number_of_pictures)

            if (event.key === Qt.Key_J || event.key === Qt.Key_Right) {
                centerIndex(selectedIndex + step)
            } else if (event.key === Qt.Key_K || event.key === Qt.Key_Left) {
                centerIndex(selectedIndex - step)
            } else if (event.key === Qt.Key_D || event.key === Qt.Key_PageDown) {
                centerIndex(selectedIndex + big)
            } else if (event.key === Qt.Key_U || event.key === Qt.Key_PageUp) {
                centerIndex(selectedIndex - big)
            } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                activateCurrent()
            } else if (event.key === Qt.Key_Escape) {
                Qt.quit()
            } else {
                return
            }

            event.accepted = true
        }
    }
}
EOF_QML
}

write_hyprquickpaper_cache() {
    cat >"$quickpaper_config/cache.sh" <<'EOF_CACHE'
#!/usr/bin/env bash
set -uo pipefail

CONFIG="$1/config.json"
wallpaper_path="$(jq -r '.wallpaper_path' "$CONFIG")"
cache_path="$(jq -r '.cache_path' "$CONFIG")"
cache_batch_size="$(jq -r '.cache_batch_size' "$CONFIG")"
if ! [[ "$cache_batch_size" =~ ^[0-9]+$ ]]; then
    cache_batch_size=4
fi

mkdir -p "$cache_path"

lock_dir="$cache_path/.cache.lock"
if ! mkdir "$lock_dir" 2>/dev/null; then
    exit 0
fi
trap 'rmdir "$lock_dir" 2>/dev/null || true' EXIT

declare -A expected_thumbs=()

thumb_command() {
    local input="$1"
    local output="$2"
    local tmp_output="${output}.tmp.$$.png"
    local mime_type

    mime_type="$(file --mime-type -b "$input" 2>/dev/null || true)"

    rm -f "$tmp_output"

    if [[ "$mime_type" =~ ^video/ ]]; then
        if command -v ffmpegthumbnailer >/dev/null 2>&1; then
            ffmpegthumbnailer -i "$input" -o "$tmp_output" -s 0 -q 8 >/dev/null 2>&1
        elif command -v ffmpeg >/dev/null 2>&1; then
            ffmpeg -y -loglevel error -ss 1 -i "$input" -frames:v 1 -vf "scale=-1:420" "$tmp_output" >/dev/null 2>&1
        else
            return 1
        fi
    elif command -v magick >/dev/null 2>&1; then
        magick "$input[0]" -thumbnail x420 -strip -quality 82 "$tmp_output"
    elif command -v convert >/dev/null 2>&1; then
        convert "$input[0]" -thumbnail x420 -strip -quality 82 "$tmp_output"
    else
        cp "$input" "$tmp_output"
    fi

    [[ -s "$tmp_output" ]] || {
        rm -f "$tmp_output"
        return 1
    }
    mv -f "$tmp_output" "$output"
}

while read -r img; do
    filename="$(basename "$img")"
    out="$cache_path/$filename.png"
    expected_thumbs["$filename.png"]=1

    [[ -s "$out" && "$out" -nt "$img" ]] && continue
    thumb_command "$img" "$out" &

    if (( cache_batch_size > 0 )); then
        while (( $(jobs -rp | wc -l) >= cache_batch_size )); do
            wait -n
        done
    fi
done < <(find "$wallpaper_path" -maxdepth 3 -type f \( \
    -iname "*.jpg" -o \
    -iname "*.jpeg" -o \
    -iname "*.png" -o \
    -iname "*.webp" -o \
    -iname "*.gif" -o \
    -iname "*.mp4" -o \
    -iname "*.webm" -o \
    -iname "*.mkv" -o \
    -iname "*.mov" \
\))

wait

find "$cache_path" -maxdepth 1 -type f -name "*.png" | while read -r thumb; do
    thumb_name="$(basename "$thumb")"
    [[ -n "${expected_thumbs[$thumb_name]:-}" ]] && continue
    rm -f "$thumb"
done
EOF_CACHE
    chmod +x "$quickpaper_config/cache.sh"
}

ensure_hyprquickpaper_config() {
    local wallpaper_path cache_path border_color background_color
    local top_margin

    [[ -d "$quickpaper_config" && -f "$quickpaper_config/shell.qml" ]] || return 1
    mkdir -p "$quickpaper_cache"

    top_margin="$selector_top_margin"
    [[ "$top_margin" =~ ^[0-9]+$ ]] || top_margin=52

    wallpaper_path="$(canonical_path "$wallpapers_dir")/"
    cache_path="$(canonical_path "$quickpaper_cache")/"
    border_color="$(theme_accent)"
    background_color="$(theme_qml_alpha_color overlay-bg "#1e1e2e" "47")"
    border_color="${border_color:-#8cb8e4}"
    background_color="${background_color:-#471e1e2e}"

    cat >"$quickpaper_config/config.json" <<EOF
{
    "wallpaper_path": "$(json_escape "$wallpaper_path")",
    "cache_path": "$(json_escape "$cache_path")",
    "number_of_pictures": 7,
    "top_margin": $top_margin,
    "border_color": "$border_color",
    "background_color": "$background_color",
    "cache_batch_size": 8
}
EOF

    cat >"$quickpaper_config/commands.sh" <<EOF
#!/usr/bin/env bash
set -uo pipefail
exec "$apply_script" "\$1"
EOF
    chmod +x "$quickpaper_config/commands.sh"
    write_hyprquickpaper_shell
    write_hyprquickpaper_cache
}

try_hyprquickpaper() {
    [[ "$selector" != "rofi" ]] || return 1
    command -v quickshell >/dev/null 2>&1 || return 1
    ensure_hyprquickpaper_config || return 1

    bash "$quickpaper_config/cache.sh" "$quickpaper_config" >/dev/null 2>&1 || true
    quickshell --no-duplicate -c hyprquickpaper
    exit 0
}

try_hyprquickpaper

tmp_map="$(mktemp)"
trap 'rm -f "$tmp_map"' EXIT

while true; do
    : >"$tmp_map"
    current="$(canonical_path "$(current_wallpaper)")"
    selected_row=0
    mapfile -t wallpaper_list < <(wallpaper_files)

    lines=()
    if ((${#wallpaper_list[@]} > 0)); then
        index=0
        for file in "${wallpaper_list[@]}"; do
            canonical_file="$(canonical_path "$file")"
            rel="${file#"$wallpapers_dir"/}"
            label="${rel%.*}"
            if is_video_path "$file"; then
                label="  $label (Live)"
            fi
            if [[ -n "$current" && "$canonical_file" == "$current" ]]; then
                label="󰄬  $label"
                selected_row="$index"
            fi
            lines+=("$label|$file")
            index=$((index + 1))
        done
    fi

    choice="$(
        {
            count=${#lines[@]}
            if (( count > 0 )); then
                for ((i=0; i<count; i++)); do
                    item="${lines[i]}"
                    lbl="${item%|*}"
                    fl="${item#*|}"
                    printf '%s\t%s\n' "$lbl" "$fl" >>"$tmp_map"
                    printf '%s\0icon\x1f%s\n' "$lbl" "$fl"
                done
            else
                printf 'No wallpapers found\0icon\x1fdialog-error\n'
            fi

            printf 'Random wallpaper\0icon\x1fmedia-playlist-shuffle\n'
            printf 'Regenerate current theme\0icon\x1fview-refresh\n'
            printf 'Open wallpaper folder\0icon\x1fuser-home\n'
            printf 'Back\0icon\x1fgo-previous\n'
        } |
            rofi -dmenu -i -matching fuzzy -show-icons \
                -p "Wallpaper" \
                -selected-row "$selected_row" \
                -theme "$theme"
    )"

    [[ -z "$choice" ]] && exit 0

    case "$choice" in
        "No wallpapers found")
            open_wallpaper_dir
            exit 0
            ;;
        "Random wallpaper")
            "$HOME/.config/anto426/wallpaper_random.sh"
            exit 0
            ;;
        "Regenerate current theme")
            current="$(current_wallpaper)"
            if [[ -n "$current" && -f "$current" ]]; then
                "$HOME/.config/anto426/wallpaper_effects.sh" "$current"
                notify "Theme regenerated"
            else
                notify "Current wallpaper not found"
            fi
            exit 0
            ;;
        "Open wallpaper folder")
            open_wallpaper_dir
            exit 0
            ;;
        "Back")
            go_back
            ;;
        *)
            selected_path="$(awk -F'\t' -v label="$choice" '$1 == label {print $2; exit}' "$tmp_map")"
            [[ -n "$selected_path" && -f "$selected_path" ]] || {
                notify "Wallpaper not found"
                exit 1
            }
            "$apply_script" "$selected_path"
            exit 0
            ;;
    esac
done
