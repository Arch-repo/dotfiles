#!/usr/bin/env bash
set -uo pipefail
export PATH="$HOME/.config/anto426/bin:$PATH"
export QML_XHR_ALLOW_FILE_READ=1

wallpapers_dir="${ANTO426_WALLPAPERS_DIR:-$HOME/Pictures/Wallpapers}"
theme="$HOME/.config/rofi/control_menu.rasi"
apply_script="$HOME/.config/anto426/wallpaper_apply.sh"
depth="${ANTO426_WALLPAPER_DEPTH:-3}"
quickpaper_config="${ANTO426_HYPRQUICKPAPER_CONFIG:-$HOME/.config/quickshell/hyprquickpaper}"
quickpaper_cache="${ANTO426_HYPRQUICKPAPER_CACHE:-$HOME/.cache/quickshell/hyprquickpaper/thumbs}"
quickpaper_src="${ANTO426_HYPRQUICKPAPER_SRC:-$HOME/Git/arch/hyprquickpaper}"
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
    local source_qml="$quickpaper_src/shell.qml"
    local target_qml="$quickpaper_config/shell.qml"

    if [[ -f "$source_qml" ]]; then
        if [[ "$(readlink -f "$source_qml" 2>/dev/null || printf '%s' "$source_qml")" != "$(readlink -f "$target_qml" 2>/dev/null || printf '%s' "$target_qml")" ]]; then
            install -m 644 "$source_qml" "$target_qml"
        fi
        return 0
    fi

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
    property int stripHeight: Math.min(700, Math.max(300, Math.round(Screen.height * 0.38)))
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

    property var colorsMap: ({})
    property string centeredBackgroundSource: ""

    function loadColorsMap() {
        if (!configs.cache_path || configs.cache_path.length === 0)
            return;
            
        var xhr = new XMLHttpRequest();
        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                if (xhr.status === 200 || xhr.status === 0) {
                    try {
                        const parsed = JSON.parse(xhr.responseText)
                        if (parsed && typeof parsed === "object")
                            colorsMap = parsed
                    } catch (e) {
                        return
                    }
                }
            }
        }
        xhr.open("GET", "file://" + configs.cache_path + "colors.json", true);
        xhr.send();
    }

    Component.onCompleted: {
        list.requestInitialCenter()
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

            onCache_pathChanged: {
                loadColorsMap();
            }
        }
    }

    FileView {
        path: configs.cache_path ? configs.cache_path + "colors.json" : ""
        watchChanges: true
        onFileChanged: {
            loadColorsMap();
        }
    }

    FolderListModel {
        id: folderModel
        folder: "file://" + configs.wallpaper_path
        showDirs: false
        nameFilters: ["*.png", "*.jpg", "*.jpeg", "*.webp", "*.gif", "*.mp4", "*.webm", "*.mkv", "*.mov", "*.MP4", "*.WEBM", "*.MKV", "*.MOV"]
        sortField: FolderListModel.Name
    }

    // Full screen overlay background
    Rectangle {
        anchors.fill: parent
        color: "#d011111b"
    }

    Image {
        id: centeredBackdrop
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        source: main.centeredBackgroundSource
        fillMode: Image.PreserveAspectCrop
        horizontalAlignment: Image.AlignHCenter
        verticalAlignment: Image.AlignVCenter
        asynchronous: true
        cache: true
        smooth: true
        opacity: main.centeredBackgroundSource.length > 0 ? 0.34 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: configs.background_color && configs.background_color.length > 0 ? configs.background_color : "#471e1e2e"
    }

    Rectangle {
        id: dock
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: Math.round(main.reservedTop * 0.5)
        anchors.leftMargin: 0
        anchors.rightMargin: 0
        height: main.stripHeight + 160
        color: "transparent"

        ListView {
            id: list
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 0
            anchors.rightMargin: 0
            anchors.topMargin: 20
            anchors.bottomMargin: 20
            focus: true

            model: folderModel
            orientation: ListView.Horizontal
            spacing: -Math.round(cardHeight * 0.07)
            clip: false
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
            property real cardHeight: Math.max(260, height - 80)
            property real activeWidth: Math.round(cardHeight * 16 / 9)
            property real inactiveWidth: Math.round(cardHeight * 0.6)
            property real tileWidth: activeWidth
            property real sideInset: Math.max(0, width * 0.5 - activeWidth * 0.5)

            header: Item {
                width: list.sideInset
                height: list.height
            }

            footer: Item {
                width: list.sideInset
                height: list.height
            }

            function wrapIndex(i) {
                if (count <= 0) return 0
                return (i + count) % count
            }

            // Smoother center placement with layout verification
            function centerIndex(i) {
                selectedIndex = wrapIndex(i)
                currentIndex = selectedIndex
                updateCenteredBackground()
                positionViewAtIndex(selectedIndex, ListView.Center)
                Qt.callLater(function() {
                    positionViewAtIndex(selectedIndex, ListView.Center)
                    updateCenteredBackground()
                })
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
                const i = indexAt(contentX + width * 0.5, height * 0.5)
                return i >= 0 ? i : selectedIndex
            }

            function isLiveFile(name) {
                const lower = String(name).toLowerCase()
                return lower.endsWith(".mp4") || lower.endsWith(".webm") || lower.endsWith(".mkv") || lower.endsWith(".mov")
            }

            function previewKey(name) {
                return String(name) + ".png"
            }

            function colorFor(name) {
                const imageKey = String(name)
                const previewImageKey = previewKey(name)
                return main.colorsMap[imageKey] || main.colorsMap[previewImageKey] || configs.border_color
            }

            function cachedPreview(name) {
                return "file://" + configs.cache_path + encodeURIComponent(previewKey(name))
            }

            function previewSource(name) {
                return cachedPreview(name)
            }

            function updateCenteredBackground() {
                if (count <= 0 || selectedIndex < 0) {
                    main.centeredBackgroundSource = ""
                    return
                }

                const name = folderModel.get(selectedIndex, "fileName")
                main.centeredBackgroundSource = name ? previewSource(name) : ""
            }

            Behavior on contentX {
                NumberAnimation {
                    duration: main.speed
                    easing.type: Easing.OutCubic
                }
            }

            property bool initialPositioned: false
            property int initialCenterPasses: 0

            Timer {
                id: initialCenterTimer
                interval: 60
                repeat: true
                onTriggered: list.ensureInitialCenter()
            }

            function requestInitialCenter() {
                initialCenterPasses = 0
                if (!initialCenterTimer.running)
                    initialCenterTimer.start()
            }

            function ensureInitialCenter() {
                if (count <= 0 || width <= 0 || activeWidth <= 0) {
                    return
                }
                selectedIndex = 0
                currentIndex = 0
                updateCenteredBackground()
                positionViewAtIndex(0, ListView.Center)
                initialPositioned = true
                initialCenterPasses += 1
                if (initialCenterPasses >= 12)
                    initialCenterTimer.stop()
            }

            onCountChanged: {
                requestInitialCenter()
                Qt.callLater(function() { updateCenteredBackground() })
            }

            onWidthChanged: {
                if (initialPositioned)
                    Qt.callLater(function() { centerIndex(selectedIndex) })
                else
                    requestInitialCenter()
            }

            onHeightChanged: {
                if (initialPositioned)
                    Qt.callLater(function() { centerIndex(selectedIndex) })
                else
                    requestInitialCenter()
            }

            delegate: Item {
                id: delegateContainer
                property bool active: index === list.selectedIndex
                
                width: active ? list.activeWidth : list.inactiveWidth
                height: list.height
                z: active ? 10 : 0

                Behavior on width {
                    NumberAnimation {
                        duration: 250
                        easing.type: Easing.OutCubic
                    }
                }

                Item {
                    id: card
                    anchors.centerIn: parent
                    width: parent.width
                    height: list.cardHeight
                    scale: active ? 1.05 : 0.95
                    transformOrigin: Item.Center

                    property bool active: delegateContainer.active
                    property bool triedOriginalFallback: false
                    property bool hadPreview: false
                    property int previewRetries: 0
                    property color activeColor: list.colorFor(fileName)

                    Behavior on scale {
                        NumberAnimation {
                            duration: 250
                            easing.type: Easing.OutCubic
                        }
                    }

                    // Glow Aura behind the card
                    Rectangle {
                        id: glowAura
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 36
                        anchors.margins: -12
                        color: card.activeColor
                        opacity: card.active ? 0.28 : 0.0
                        radius: 26
                        z: -1
                        transform: Shear { xFactor: -0.25 }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 250
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    // Rounded image container
                    Rectangle {
                        id: imgContainer
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 36
                        radius: 20
                        clip: true
                        color: "transparent"
                        transform: Shear { xFactor: -0.25 }

                        Image {
                            id: img
                            anchors.fill: parent
                            anchors.margins: card.active ? 3 : 2
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                            cache: true
                            smooth: true
                            source: list.previewSource(fileName)
                            visible: true

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
                    }

                    // Text placeholder if preview fails
                    Text {
                        id: alt
                        z: 2
                        visible: !card.hadPreview && img.status !== Image.Ready
                        text: img.status === Image.Error && card.previewRetries >= 10 ? "No preview" : "Caricamento..."
                        color: configs.border_color
                        anchors.centerIn: parent
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "JetBrainsMono Nerd Font"
                    }

                    // Premium active glow border
                    Rectangle {
                        z: 10
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 36
                        color: "transparent"
                        visible: card.active
                        border.width: 3
                        border.color: card.activeColor
                        radius: 20
                        transform: Shear { xFactor: -0.25 }
                    }

                    // Elegant indicator badge for live video wallpapers
                    Rectangle {
                        z: 11
                        visible: list.isLiveFile(fileName)
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        width: 58
                        height: 22
                        radius: 11
                        color: "#a011111b"
                        border.width: 1
                        border.color: configs.border_color

                        Text {
                            anchors.centerIn: parent
                            text: "LIVE"
                            color: "white"
                            font.pixelSize: 9
                            font.bold: true
                            font.family: "JetBrainsMono Nerd Font"
                        }
                    }

                    // Elegant title below the card, visible only if active
                    Text {
                        id: titleText
                        anchors.bottom: parent.bottom
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(parent.width, Screen.width * 0.9)
                        text: {
                            var base = fileName.split(".")[0];
                            var words = base.replace(/[-_]/g, " ").split(" ");
                            for (var i = 0; i < words.length; i++) {
                                words[i] = words[i].charAt(0).toUpperCase() + words[i].slice(1);
                            }
                            return words.join(" ");
                        }
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        font.pixelSize: 13
                        font.bold: true
                        font.family: "JetBrainsMono Nerd Font"
                        opacity: card.active ? 0.9 : 0.0

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 200
                                easing.type: Easing.OutCubic
                            }
                        }
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

    // Translucent help bar at the bottom
    Rectangle {
        id: helpBar
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: Math.max(20, Screen.height * 0.04)
        width: helpContent.implicitWidth + 32
        height: 38
        color: "#9011111b"
        border.width: 1
        border.color: "#25ffffff"
        radius: 19

        Row {
            id: helpContent
            anchors.centerIn: parent
            spacing: 16

            Text {
                text: "󰄾󰄼 Naviga"
                color: "#cdd6f4"
                font.pixelSize: 11
                font.bold: true
                font.family: "JetBrainsMono Nerd Font"
            }

            Text {
                text: "•"
                color: "#585b70"
                font.pixelSize: 11
            }

            Text {
                text: "󰌑 Applica"
                color: configs.border_color
                font.pixelSize: 11
                font.bold: true
                font.family: "JetBrainsMono Nerd Font"
            }

            Text {
                text: "•"
                color: "#585b70"
                font.pixelSize: 11
            }

            Text {
                text: "󱊷 Chiudi"
                color: "#f38ba8"
                font.pixelSize: 11
                font.bold: true
                font.family: "JetBrainsMono Nerd Font"
            }
        }
    }
}
EOF_QML
}

write_hyprquickpaper_cache() {
    local source_cache="$quickpaper_src/cache.sh"
    local target_cache="$quickpaper_config/cache.sh"

    if [[ -f "$source_cache" ]]; then
        if [[ "$(readlink -f "$source_cache" 2>/dev/null || printf '%s' "$source_cache")" != "$(readlink -f "$target_cache" 2>/dev/null || printf '%s' "$target_cache")" ]]; then
            install -m 755 "$source_cache" "$target_cache"
        else
            chmod +x "$target_cache"
        fi
        return 0
    fi

    cat >"$quickpaper_config/cache.sh" <<'EOF_CACHE'
#!/usr/bin/env bash
set -uo pipefail

CONFIG="$1/config.json"
wallpaper_path="$(jq -r '.wallpaper_path' "$CONFIG")"
cache_path="$(jq -r '.cache_path' "$CONFIG")"
cache_batch_size="$(jq -r '.cache_batch_size' "$CONFIG")"
wallpaper_core="${ANTO426_WALLPAPER_CORE:-$HOME/.config/anto426/wallpaper_core}"
color_engine_version="core-preview-v1"
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

fallback_color() {
    local image="$1"
    local hex

    hex=$(magick "$image" -resize 1x1 -format "%[hex:u]" info: 2>/dev/null || convert "$image" -resize 1x1 -format "%[hex:u]" info: 2>/dev/null || echo "8cb8e4")
    hex="$(printf '%s' "$hex" | tr -d '#' | tr '[:lower:]' '[:upper:]')"
    printf '#%s\n' "${hex:0:6}"
}

write_color_file() {
    local input="$1"
    local preview="$2"
    local color_file="$3"
    local tmp_color="${color_file}.tmp.$$"
    local tmp_meta="${color_file}.engine.tmp.$$"
    local hex
    local engine="$color_engine_version"

    if [[ -x "$wallpaper_core" ]]; then
        hex="$("$wallpaper_core" preview-color "$input" accent 2>/dev/null | head -n1 || true)"
    fi

    if [[ ! "$hex" =~ ^#[[:xdigit:]]{6}$ ]]; then
        hex="$(fallback_color "$preview")"
        engine="fallback-v1"
    fi

    printf '%s\n' "${hex^^}" >"$tmp_color"
    mv -f "$tmp_color" "$color_file"
    printf '%s\n' "$engine" >"$tmp_meta"
    mv -f "$tmp_meta" "$color_file.engine"
}

thumb_command() {
    local input="$1"
    local output="$2"
    local tmp_output="${output}.tmp.$$.png"
    local mime_type

    mime_type="$(file --mime-type -b "$input" 2>/dev/null || true)"

    rm -f "$tmp_output"

    if [[ "$mime_type" =~ ^video/ ]]; then
        if command -v ffmpegthumbnailer >/dev/null 2>&1; then
            ffmpegthumbnailer -i "$input" -o "$tmp_output" -s 800 -q 8 >/dev/null 2>&1
        elif command -v ffmpeg >/dev/null 2>&1; then
            ffmpeg -y -loglevel error -ss 1 -i "$input" -frames:v 1 -vf "scale=-1:800" "$tmp_output" >/dev/null 2>&1
        else
            return 1
        fi
    elif command -v magick >/dev/null 2>&1; then
        magick "$input[0]" -thumbnail x800 -strip -quality 82 "$tmp_output"
    elif command -v convert >/dev/null 2>&1; then
        convert "$input[0]" -thumbnail x800 -strip -quality 82 "$tmp_output"
    else
        cp "$input" "$tmp_output"
    fi

    [[ -s "$tmp_output" ]] || {
        rm -f "$tmp_output"
        return 1
    }
    mv -f "$tmp_output" "$output"

    write_color_file "$input" "$output" "${output%.png}.accent"
}

while read -r img; do
    filename="$(basename "$img")"
    out="$cache_path/$filename.png"
    expected_thumbs["$filename.png"]=1

    if [[ -s "$out" && "$out" -nt "$img" ]]; then
        color_file="${out%.png}.accent"
        color_meta="$color_file.engine"
        if [[ ! -s "$color_file" || "$color_file" -ot "$img" || "$(cat "$color_meta" 2>/dev/null || true)" != "$color_engine_version" ]]; then
            write_color_file "$img" "$out" "$color_file" &
        fi
        continue
    fi
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

# Combine all palette accent files into colors.json atomically.
json_tmp="$cache_path/colors.json.tmp.$$"
printf '{\n' >"$json_tmp"
first=1
while read -r color_file; do
    filename="$(basename "${color_file%.accent}")"
    color_val="$(cat "$color_file" 2>/dev/null || echo "#8cb8e4")"
    if (( first )); then
        first=0
    else
        printf ',\n' >>"$json_tmp"
    fi
    key_json="$(jq -Rn --arg s "$filename" '$s')"
    val_json="$(jq -Rn --arg s "$color_val" '$s')"
    printf '  %s: %s' "$key_json" "$val_json" >>"$json_tmp"
done < <(find "$cache_path" -maxdepth 1 -type f -name "*.accent" | sort -f)
printf '\n}\n' >>"$json_tmp"
mv -f "$json_tmp" "$cache_path/colors.json"

find "$cache_path" -maxdepth 1 -type f -name "*.png" | while read -r thumb; do
    thumb_name="$(basename "$thumb")"
    [[ -n "${expected_thumbs[$thumb_name]:-}" ]] && continue
    rm -f "$thumb" "${thumb%.png}.accent" "${thumb%.png}.accent.engine" "${thumb%.png}.color"
done
EOF_CACHE
    chmod +x "$quickpaper_config/cache.sh"
}

ensure_hyprquickpaper_config() {
    local wallpaper_path cache_path border_color background_color
    local top_margin

    mkdir -p "$quickpaper_config" "$quickpaper_cache"

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

if ! command -v quickshell >/dev/null 2>&1; then
    notify "quickshell non è installato per il selettore sfondi!"
    exit 1
fi

ensure_hyprquickpaper_config
bash "$quickpaper_config/cache.sh" "$quickpaper_config" >/dev/null 2>&1 || true
quickpaper_pid=""
trap '[[ -n "${quickpaper_pid:-}" ]] && kill "$quickpaper_pid" 2>/dev/null || true; release_selector_lock; exit 130' INT TERM
quickshell --no-duplicate -c hyprquickpaper &
quickpaper_pid="$!"
wait "$quickpaper_pid"
status="$?"
release_selector_lock
exit "$status"
