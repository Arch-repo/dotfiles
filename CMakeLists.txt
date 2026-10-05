cmake_minimum_required(VERSION 3.25)
project(AntoDesktop VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(DESIGN_DIR ${CMAKE_BINARY_DIR}/design)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/design.py ${DESIGN_DIR}
  COMMAND_ERROR_IS_FATAL ANY)
file(GLOB DESIGN_INPUTS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/native/menu/assets/components/*.css.in
  ${CMAKE_SOURCE_DIR}/native/menu/assets/*.css.in ${CMAKE_SOURCE_DIR}/native/shell/assets/*.css.in ${CMAKE_SOURCE_DIR}/native/common/assets/*.css.in ${CMAKE_SOURCE_DIR}/native/widgets/assets/*.css.in)
add_custom_command(OUTPUT ${DESIGN_DIR}/design_tokens.h ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css ${DESIGN_DIR}/application-material.json
  ${DESIGN_DIR}/widgets.css ${DESIGN_DIR}/wallpaper.css ${DESIGN_DIR}/osd.css ${DESIGN_DIR}/glass.css ${DESIGN_DIR}/waybar.css ${DESIGN_DIR}/swaync.css
  ${DESIGN_DIR}/terminal.conf ${DESIGN_DIR}/terminal-hypr.conf ${DESIGN_DIR}/Tokens.qml
  ${DESIGN_DIR}/hyprlock.conf ${DESIGN_DIR}/hyprlock-defaults.conf
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/design.py ${DESIGN_DIR}
  DEPENDS design/tokens.json design/surface-controls.json scripts/design.py native/lock/hyprlock.conf.in ${DESIGN_INPUTS} VERBATIM)
add_custom_target(anto-design DEPENDS ${DESIGN_DIR}/design_tokens.h ${DESIGN_DIR}/menu.css)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED IMPORTED_TARGET gtk4>=4.22 gtk4-layer-shell-0>=1.3 gio-unix-2.0 json-c wayland-client)
pkg_check_modules(CORE REQUIRED IMPORTED_TARGET gio-2.0 json-c)
add_compile_options(-Wall -Wextra -Wformat=2 -Wshadow)
add_library(anto-state STATIC native/common/src/local_config.c)
target_include_directories(anto-state PUBLIC native/common/include)
target_link_libraries(anto-state PUBLIC PkgConfig::CORE)
add_library(anto-layout STATIC native/common/src/layout.cpp)
target_include_directories(anto-layout PUBLIC native/common/include)
target_include_directories(anto-layout PUBLIC ${DESIGN_DIR})
add_dependencies(anto-layout anto-design)

find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/protocols)
set(GLASS_PROTOCOLS)
foreach(protocol ext-background-effect-v1 hyprglass-item-v1)
  set(xml ${CMAKE_SOURCE_DIR}/native/common/protocols/${protocol}.xml)
  set(header ${CMAKE_BINARY_DIR}/protocols/${protocol}-client.h)
  set(code ${CMAKE_BINARY_DIR}/protocols/${protocol}.c)
  add_custom_command(OUTPUT ${header} ${code}
    COMMAND ${WAYLAND_SCANNER} client-header ${xml} ${header}
    COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${code}
    DEPENDS ${xml} VERBATIM)
  list(APPEND GLASS_PROTOCOLS ${header} ${code})
endforeach()
add_library(anto-gtk STATIC native/common/src/calendar_data.c native/common/src/monitor.c native/common/src/frontend.c native/common/src/size_bin.c native/common/src/palette.c native/common/src/glass.c native/common/src/ui/base.c native/common/src/ui/text.c native/common/src/ui/controls.c native/common/src/ui/surfaces.c native/common/src/ui/calendar.c native/common/src/query.c ${GLASS_PROTOCOLS})
target_include_directories(anto-gtk PUBLIC native/common/include)
target_include_directories(anto-gtk PUBLIC ${DESIGN_DIR})
add_dependencies(anto-gtk anto-design)
target_include_directories(anto-gtk PRIVATE ${CMAKE_BINARY_DIR}/protocols)
target_link_libraries(anto-gtk PUBLIC PkgConfig::GTK m)

set(MENU_PAGES audio background bluetooth bluetooth_agent brightness calendar calendar_editor capture clipboard display emoji floating hardware keyboard launcher notifications power settings shortcuts system wallpaper widgets wifi)
set(MENU_SOURCES native/menu/src/core/navigation.c native/menu/src/core/live.c native/menu/src/core/input.c native/menu/src/core/theme.c native/menu/src/core/monitor.c native/menu/src/core/window.c native/menu/src/core/context.c native/menu/src/core/components.c native/menu/src/adapters/actions.c native/menu/src/adapters/backend.c native/menu/src/adapters/direct.c native/menu/src/adapters/virtual_output.c native/menu/src/adapters/workspace_output.c)
foreach(page IN LISTS MENU_PAGES)
  foreach(unit view state actions record bindings)
    set(source native/menu/src/modules/${page}/${unit}.c)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${source})
      list(APPEND MENU_SOURCES ${source})
    endif()
  endforeach()
endforeach()
add_library(anto-menu-ui STATIC ${MENU_SOURCES})
target_sources(anto-menu-ui PRIVATE native/menu/src/ui/sidebar.c native/menu/src/ui/header.c
  native/menu/src/ui/content.c native/menu/src/ui/status.c native/menu/src/ui/bindings.c
  native/menu/src/ui/rows.c native/menu/src/ui/tiles.c native/menu/src/ui/sliders.c)
target_include_directories(anto-menu-ui PUBLIC native/menu/include PRIVATE native/menu/src/core native/menu/src/ui)
target_link_libraries(anto-menu-ui PUBLIC anto-state anto-layout anto-gtk PkgConfig::GTK m)
add_executable(anto-menu native/menu/src/core/main.c)
target_link_libraries(anto-menu PRIVATE anto-menu-ui)

set(SERVICE_DOMAINS audio bluetooth calendar capture config display energy network notes record session system)
set(SERVICE_SOURCES native/services/src/main.c native/services/src/runtime/command.c
  native/services/src/runtime/protocol.c native/services/src/runtime/storage.c native/services/src/runtime/service.c native/services/src/domains/bluetooth/bluez.c)
foreach(domain IN LISTS SERVICE_DOMAINS)
  list(APPEND SERVICE_SOURCES native/services/src/domains/${domain}/service.c native/services/src/domains/${domain}/provider.c)
endforeach()
add_executable(anto-menu-backend ${SERVICE_SOURCES} native/services/src/domains/desktop/provider.c)
foreach(unit model storage queries mutations transaction arrangement)
  target_sources(anto-menu-backend PRIVATE native/services/src/domains/display/${unit}.c)
endforeach()
target_include_directories(anto-menu-backend PRIVATE native/services/include)
target_link_libraries(anto-menu-backend PRIVATE anto-state PkgConfig::CORE m)

add_executable(anto-wallpaper-core native/wallpaper/wallpaper_core.c native/wallpaper/preferences.c)
target_link_libraries(anto-wallpaper-core PRIVATE anto-state m)
target_include_directories(anto-wallpaper-core PRIVATE ${DESIGN_DIR})
add_dependencies(anto-wallpaper-core anto-design)
add_library(anto-catalog STATIC native/wallpaper/catalog.cpp)
target_link_libraries(anto-catalog PUBLIC PkgConfig::GTK)
target_include_directories(anto-catalog PUBLIC native/wallpaper)
add_library(anto-gallery STATIC native/wallpaper/gallery.cpp
  native/wallpaper/gallery/window.cpp native/wallpaper/gallery/header.cpp native/wallpaper/gallery/toolbar.cpp
  native/wallpaper/gallery/content.cpp native/wallpaper/gallery/preview.cpp native/wallpaper/gallery/preview_backdrop.cpp native/wallpaper/gallery/footer.cpp native/wallpaper/gallery/state.cpp native/wallpaper/gallery/actions.cpp)
add_library(anto-wallpaper-jobs STATIC native/wallpaper/apply.cpp)
target_include_directories(anto-wallpaper-jobs PUBLIC native/wallpaper)
target_link_libraries(anto-wallpaper-jobs PUBLIC PkgConfig::CORE)
target_link_libraries(anto-gallery PUBLIC anto-catalog anto-gtk anto-wallpaper-jobs)
target_link_libraries(anto-menu-ui PUBLIC anto-gallery)
add_executable(anto-wallpaper native/wallpaper/main.cpp)
target_link_libraries(anto-wallpaper PRIVATE anto-gallery)
file(GLOB WIDGET_VIEWS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/native/widgets/views/*.c)
set(WIDGET_REGISTRY ${CMAKE_BINARY_DIR}/widget-registry.c)
add_custom_command(OUTPUT ${WIDGET_REGISTRY}
  COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/widgets_registry.py
    ${CMAKE_SOURCE_DIR}/native/widgets/views ${WIDGET_REGISTRY}
  DEPENDS scripts/widgets_registry.py ${WIDGET_VIEWS} VERBATIM)
add_library(anto-widget-engine STATIC ${WIDGET_REGISTRY} ${WIDGET_VIEWS}
  native/widgets/core/settings.c native/widgets/core/window.c native/widgets/core/runtime.c
  native/widgets/data/store.c native/widgets/data/hardware.c native/widgets/data/agenda.c native/widgets/data/media.c native/widgets/data/spectrum.c
  native/widgets/ui/spectrum.c)
target_include_directories(anto-widget-engine PUBLIC native/widgets)
target_link_libraries(anto-widget-engine PUBLIC anto-state anto-gtk)
add_executable(anto-widgets native/widgets/core/main.c)
target_link_libraries(anto-widgets PRIVATE anto-widget-engine)
target_link_libraries(anto-menu-ui PUBLIC anto-widget-engine)
add_executable(anto-calendar native/calendar/remote_sync_core.c)
target_link_libraries(anto-calendar PRIVATE PkgConfig::CORE)
add_executable(anto-keyboard-status native/status/keyboard_status_json.c)
target_link_libraries(anto-keyboard-status PRIVATE PkgConfig::CORE)
add_executable(workspace-triad native/status/workspace-triad.c)
target_link_libraries(workspace-triad PRIVATE PkgConfig::CORE)
add_executable(media-marquee native/status/media-marquee.c)
target_link_libraries(media-marquee PRIVATE PkgConfig::CORE)

add_executable(anto-osd native/osd/main.cpp native/osd/view.cpp native/osd/actions.cpp)
target_link_libraries(anto-osd PRIVATE anto-gtk)

install(TARGETS anto-menu anto-wallpaper anto-osd RUNTIME DESTINATION bin)
install(TARGETS anto-menu-backend RUNTIME DESTINATION libexec/anto-menu)
install(TARGETS anto-wallpaper-core anto-widgets anto-calendar anto-keyboard-status workspace-triad media-marquee RUNTIME DESTINATION libexec/anto426)
install(FILES native/menu/assets/tokens.css DESTINATION share/anto-desktop)
install(FILES ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css ${DESIGN_DIR}/widgets.css ${DESIGN_DIR}/wallpaper.css ${DESIGN_DIR}/osd.css
  ${DESIGN_DIR}/glass.css ${DESIGN_DIR}/waybar.css ${DESIGN_DIR}/swaync.css
  ${DESIGN_DIR}/terminal.conf ${DESIGN_DIR}/terminal-hypr.conf
  ${DESIGN_DIR}/application-material.json
  ${DESIGN_DIR}/hyprlock.conf ${DESIGN_DIR}/hyprlock-defaults.conf DESTINATION share/anto-desktop)
install(DIRECTORY native/login/ DESTINATION share/anto-desktop/login PATTERN "tests" EXCLUDE)
install(FILES ${DESIGN_DIR}/Tokens.qml DESTINATION share/anto-desktop/login)
install(DIRECTORY native/menu/scripts/actions DESTINATION lib/anto-menu USE_SOURCE_PERMISSIONS)

if(BUILD_TESTING)
  add_executable(query-fixture native/common/tests/query.c)
  target_link_libraries(query-fixture PRIVATE anto-gtk)
  add_test(NAME query-lifecycle COMMAND query-fixture)
  add_test(NAME shared-primitives COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/primitives-audit.py ${CMAKE_SOURCE_DIR})
  add_executable(primitives-fixture native/common/tests/primitives.c)
  target_link_libraries(primitives-fixture PRIVATE anto-gtk)
  add_test(NAME primitive-appearance COMMAND primitives-fixture ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css ${DESIGN_DIR}/widgets.css ${DESIGN_DIR}/wallpaper.css ${DESIGN_DIR}/osd.css)
  set_tests_properties(primitive-appearance PROPERTIES SKIP_RETURN_CODE 77)
  add_executable(calendar-ui-fixture native/menu/tests/calendar-ui-fixture.c
    native/menu/src/modules/calendar/view.c native/menu/src/modules/calendar/state.c
    native/menu/src/modules/calendar/actions.c native/menu/src/modules/calendar/bindings.c)
  target_include_directories(calendar-ui-fixture PRIVATE native/menu/include)
  target_link_libraries(calendar-ui-fixture PRIVATE anto-gtk)
  target_compile_options(calendar-ui-fixture PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(calendar-ui-fixture PRIVATE -Wl,--gc-sections)
  add_test(NAME calendar-ui COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    $<TARGET_FILE:calendar-ui-fixture> ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css)
  add_executable(emoji-ui-fixture native/menu/tests/emoji-ui-fixture.c
    native/menu/src/modules/emoji/view.c native/menu/src/modules/emoji/state.c
    native/menu/src/modules/emoji/actions.c)
  target_include_directories(emoji-ui-fixture PRIVATE native/menu/include)
  target_link_libraries(emoji-ui-fixture PRIVATE anto-gtk)
  target_compile_options(emoji-ui-fixture PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(emoji-ui-fixture PRIVATE -Wl,--gc-sections)
  add_test(NAME emoji-ui COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    $<TARGET_FILE:emoji-ui-fixture> ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css)
  add_executable(styles-gtk4-fixture native/common/tests/styles.c)
  target_link_libraries(styles-gtk4-fixture PRIVATE PkgConfig::GTK)
  add_dependencies(styles-gtk4-fixture anto-design)
  add_test(NAME styles-gtk4 COMMAND styles-gtk4-fixture ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    ${DESIGN_DIR}/design.css ${DESIGN_DIR}/primitives.css ${DESIGN_DIR}/menu.css ${DESIGN_DIR}/widgets.css ${DESIGN_DIR}/wallpaper.css ${DESIGN_DIR}/osd.css ${DESIGN_DIR}/glass.css ${DESIGN_DIR}/swaync.css)
  pkg_check_modules(GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
  if(GTK3_FOUND)
    add_executable(styles-gtk3-fixture native/common/tests/styles.c)
    target_link_libraries(styles-gtk3-fixture PRIVATE PkgConfig::GTK3)
    add_dependencies(styles-gtk3-fixture anto-design)
    add_test(NAME styles-gtk3 COMMAND styles-gtk3-fixture ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
      ${DESIGN_DIR}/design.css ${DESIGN_DIR}/waybar.css)
  endif()
  add_executable(live-underlay-fixture native/common/tests/live_underlay.c)
  target_link_libraries(live-underlay-fixture PRIVATE PkgConfig::GTK)
  add_executable(glass-lifecycle-fixture native/common/tests/glass-lifecycle.c)
  target_link_libraries(glass-lifecycle-fixture PRIVATE anto-gtk)
  add_test(NAME glass-lifecycle COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css $<TARGET_FILE:glass-lifecycle-fixture>)
  add_executable(shell-alpha-fixture native/common/tests/shell-alpha.c)
  target_link_libraries(shell-alpha-fixture PRIVATE anto-gtk)
  add_test(NAME shell-alpha COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    $<TARGET_FILE:shell-alpha-fixture> ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    ${DESIGN_DIR}/design.css ${DESIGN_DIR}/waybar.css ${DESIGN_DIR}/swaync.css)
  add_executable(background-fixture native/wallpaper/tests/apply.cpp)
  target_link_libraries(background-fixture PRIVATE anto-wallpaper-jobs)
  add_test(NAME wallpaper-background COMMAND python3 ${CMAKE_SOURCE_DIR}/native/wallpaper/tests/background.py $<TARGET_FILE:background-fixture>)
  add_executable(gallery-ui-fixture native/wallpaper/tests/gallery-ui.cpp)
  target_link_libraries(gallery-ui-fixture PRIVATE anto-gallery)
  add_test(NAME gallery-ui COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css $<TARGET_FILE:gallery-ui-fixture>)
  add_executable(menu-geometry-fixture native/menu/tests/menu-geometry.c native/menu/src/core/window.c
    native/menu/src/ui/sidebar.c native/menu/src/ui/header.c native/menu/src/ui/content.c native/menu/src/ui/status.c)
  target_include_directories(menu-geometry-fixture PRIVATE native/menu/include native/menu/src/ui native/menu/src/core)
  target_link_libraries(menu-geometry-fixture PRIVATE anto-gtk)
  add_test(NAME menu-geometry COMMAND python3 ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css $<TARGET_FILE:menu-geometry-fixture>)
  add_executable(wallpaper-menu-fixture native/menu/tests/wallpaper-menu.c)
  target_link_libraries(wallpaper-menu-fixture PRIVATE anto-menu-ui)
  add_test(NAME wallpaper-menu COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    $<TARGET_FILE:wallpaper-menu-fixture> $<TARGET_FILE:anto-wallpaper>)
  add_executable(size-bin-fixture native/common/tests/size_bin.c)
  target_link_libraries(size-bin-fixture PRIVATE anto-gtk)
  add_test(NAME gallery-bounds COMMAND size-bin-fixture)
  set_tests_properties(gallery-bounds PROPERTIES SKIP_RETURN_CODE 77)
  add_test(NAME palette-multi-output COMMAND bash ${CMAKE_SOURCE_DIR}/support/tests/multi_wallpaper_palette_fixture.sh $<TARGET_FILE:anto-wallpaper-core>)
  set(POINTER_XML ${CMAKE_SOURCE_DIR}/native/widgets/tests/protocols/wlr-virtual-pointer.xml)
  add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/protocols/wlr-virtual-pointer-client.h ${CMAKE_BINARY_DIR}/protocols/wlr-virtual-pointer.c
    COMMAND ${WAYLAND_SCANNER} client-header ${POINTER_XML} ${CMAKE_BINARY_DIR}/protocols/wlr-virtual-pointer-client.h
    COMMAND ${WAYLAND_SCANNER} private-code ${POINTER_XML} ${CMAKE_BINARY_DIR}/protocols/wlr-virtual-pointer.c
    DEPENDS ${POINTER_XML} VERBATIM)
  add_executable(widget-pointer-fixture native/widgets/tests/pointer.c ${CMAKE_BINARY_DIR}/protocols/wlr-virtual-pointer.c)
  target_include_directories(widget-pointer-fixture PRIVATE ${CMAKE_BINARY_DIR}/protocols)
  target_link_libraries(widget-pointer-fixture PRIVATE PkgConfig::GTK)
  add_test(NAME widget-registry COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/widgets/tests/registry.py ${CMAKE_SOURCE_DIR}/scripts/widgets_registry.py)
  add_executable(widget-state-fixture native/widgets/tests/state.c)
  target_link_libraries(widget-state-fixture PRIVATE anto-widget-engine)
  add_test(NAME widget-state COMMAND widget-state-fixture)
  add_executable(widget-ui-fixture native/widgets/tests/ui.c)
  target_link_libraries(widget-ui-fixture PRIVATE anto-widget-engine)
  add_test(NAME widget-ui COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    dbus-run-session -- $<TARGET_FILE:widget-ui-fixture>)
  add_test(NAME widget-lifecycle COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/common/tests/gtk-isolated.py
    --assets ${DESIGN_DIR} --tokens ${CMAKE_SOURCE_DIR}/native/menu/assets/tokens.css
    dbus-run-session -- ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/widgets/tests/lifecycle.py $<TARGET_FILE:anto-widgets>)
  add_test(NAME app-themes COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/support/tests/app_themes.py)
  add_executable(launcher-theme-fixture native/menu/tests/launcher-theme.c native/menu/src/modules/launcher/actions.c)
  target_include_directories(launcher-theme-fixture PRIVATE native/menu/include native/common/include)
  target_link_libraries(launcher-theme-fixture PRIVATE PkgConfig::GTK)
  add_test(NAME launcher-theme COMMAND dbus-run-session -- $<TARGET_FILE:launcher-theme-fixture>)
  add_test(NAME theme-reload COMMAND bash ${CMAKE_SOURCE_DIR}/support/tests/theme_reload_fixture.sh)
  add_test(NAME boot-login-commit COMMAND bash ${CMAKE_SOURCE_DIR}/support/tests/boot_login_commit_fixture.sh)
  add_test(NAME boot-menu-flow COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/support/tests/boot_menu_flow.py $<TARGET_FILE:anto-wallpaper-core>)

  add_executable(layout-fixture native/common/tests/layout.cpp)
  target_link_libraries(layout-fixture PRIVATE anto-layout)
  add_test(NAME layout COMMAND layout-fixture)
  add_executable(catalog-fixture native/wallpaper/tests/catalog.cpp)
  target_link_libraries(catalog-fixture PRIVATE anto-catalog)
  add_test(NAME catalog COMMAND catalog-fixture)
  add_executable(bluetooth-ui-fixture native/menu/tests/bluetooth-ui-fixture.c)
  foreach(page bluetooth bluetooth_agent)
    foreach(unit view state actions bindings)
      if(EXISTS ${CMAKE_SOURCE_DIR}/native/menu/src/modules/${page}/${unit}.c)
        target_sources(bluetooth-ui-fixture PRIVATE native/menu/src/modules/${page}/${unit}.c)
      endif()
    endforeach()
  endforeach()
  target_include_directories(bluetooth-ui-fixture PRIVATE native/menu/include native/common/include)
  target_link_libraries(bluetooth-ui-fixture PRIVATE anto-gtk)
  add_test(NAME static-writers COMMAND bash ${CMAKE_SOURCE_DIR}/native/menu/tests/static-writer-audit.sh)
  add_test(NAME backend COMMAND bash ${CMAKE_SOURCE_DIR}/native/menu/tests/backend-fixture.sh $<TARGET_FILE:anto-menu-backend>)
  add_test(NAME service-contracts COMMAND python3 ${CMAKE_SOURCE_DIR}/native/menu/tests/service-contracts.py $<TARGET_FILE:anto-menu-backend>)
  add_test(NAME calendar-display COMMAND bash ${CMAKE_SOURCE_DIR}/native/menu/tests/calendar-display-fixture.sh $<TARGET_FILE:anto-menu-backend>)
  add_test(NAME bluetooth COMMAND python3 ${CMAKE_SOURCE_DIR}/native/menu/tests/bluetooth-bluez-fixture.py $<TARGET_FILE:anto-menu-backend> $<TARGET_FILE:bluetooth-ui-fixture> ${CMAKE_SOURCE_DIR}/native/menu/tests/fixtures/bluetooth-ui-backend.sh)
  add_test(NAME virtual-output COMMAND bash ${CMAKE_SOURCE_DIR}/native/menu/tests/virtual-output-fixture.sh $<TARGET_FILE:anto-menu>)
  add_test(NAME workspace-output COMMAND bash ${CMAKE_SOURCE_DIR}/native/menu/tests/workspace-output-watch-fixture.sh $<TARGET_FILE:anto-menu>)
  # Broadway maps port zero to its default HTTP port, including fixtures
  # whose Unix sockets and data directories are otherwise isolated.
  set_tests_properties(calendar-ui emoji-ui glass-lifecycle shell-alpha gallery-ui
    menu-geometry wallpaper-menu widget-ui widget-lifecycle bluetooth PROPERTIES RESOURCE_LOCK gtk_broadway_server)
endif()

if(BUILD_TESTING)
  find_package(Qt6 6.5 REQUIRED COMPONENTS Quick Qml Gui Test)
  add_executable(login-ui-fixture native/login/tests/fixture.cpp)
  set_target_properties(login-ui-fixture PROPERTIES AUTOMOC ON)
  target_link_libraries(login-ui-fixture PRIVATE Qt6::Quick Qt6::Qml Qt6::Gui Qt6::Test)
  add_test(NAME login-qt6 COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/native/login/tests/run.py
    $<TARGET_FILE:login-ui-fixture> ${CMAKE_SOURCE_DIR}/native/login ${DESIGN_DIR}/Tokens.qml)
endif()
