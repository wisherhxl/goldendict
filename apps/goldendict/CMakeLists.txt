# SPDX-License-Identifier: GPL-3.0-or-later

ti_add_qt_app(goldendict MODULES core network
  EXTRAS ${qt_link} ${qt_print_support_link})
target_compile_definitions(goldendict PRIVATE
  GOLDENDICT_APPLICATION_VERSION="${TI_VERSION_MAJOR}.${TI_VERSION_MINOR}.${TI_VERSION_REVISION}")
if(UNIX AND NOT APPLE AND TIGER_INSTALL_RUNTIME_DEPENDENCIES)
  set_target_properties(goldendict PROPERTIES
    INSTALL_RPATH "\$ORIGIN/../${TIGER_LIB_INSTALL_PATH};\$ORIGIN"
    INSTALL_RPATH_USE_LINK_PATH FALSE)
  foreach(runtime_target IN ITEMS goldendict_core goldendict_tiger)
    if(TARGET ${runtime_target})
      set_target_properties(${runtime_target} PROPERTIES
        INSTALL_RPATH "\$ORIGIN"
        INSTALL_RPATH_USE_LINK_PATH FALSE)
    endif()
  endforeach()
  # Qt WebEngine links HarfBuzz subset from the Conan graph. CMake's runtime
  # scanner can otherwise pair it with an ABI-incompatible system HarfBuzz
  # having the same SONAME, so keep the matched Conan main library with it.
  if(TARGET harfbuzz::harfbuzz AND TARGET harfbuzz::subset)
    _ti_collect_runtime_dependency_dirs(runtime_dependency_dirs)
    find_library(harfbuzz_main_library
      NAMES harfbuzz
      PATHS ${runtime_dependency_dirs}
      NO_DEFAULT_PATH
      NO_CACHE)
    if(NOT harfbuzz_main_library)
      message(FATAL_ERROR
        "Could not resolve the HarfBuzz main library from runtime dependency directories")
    endif()
    install(CODE
      "file(INSTALL
        DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${TIGER_LIB_INSTALL_PATH}\"
        TYPE SHARED_LIBRARY
        FILES \"${harfbuzz_main_library}\"
        FOLLOW_SYMLINK_CHAIN)"
      COMPONENT libs)
  endif()
endif()

ti_add_executable(legacy_configuration_location_test
  tests/legacy_configuration_location_test.cpp
  src/legacy_configuration_location.cpp)
ti_target_include_modules_recurse(legacy_configuration_location_test
  goldendict_core)
ti_target_link_libraries(legacy_configuration_location_test
  goldendict_core ${QT_NAMESPACE}::Test)
add_test(NAME legacy_configuration_location_test
  COMMAND legacy_configuration_location_test)

add_test(NAME goldendict_smoke COMMAND $<TARGET_FILE:goldendict> --smoke)

if(UNIX AND NOT APPLE AND TIGER_INSTALL_RUNTIME_DEPENDENCIES)
  add_test(NAME goldendict_installed_runtime_smoke
    COMMAND ${CMAKE_COMMAND}
      -DBUILD_DIRECTORY=${CMAKE_BINARY_DIR}
      -DINSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/installed-runtime
      -DINSTALL_LIBDIR=${TIGER_LIB_INSTALL_PATH}
      -DREADELF_EXECUTABLE=${CMAKE_READELF}
      -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/installed-runtime-test-home
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/installed_runtime_smoke.cmake)
  set_tests_properties(goldendict_installed_runtime_smoke PROPERTIES TIMEOUT 60)
endif()

add_test(NAME goldendict_file_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --file-menu-smoke)
set_tests_properties(goldendict_file_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/file-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/file-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_search_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --search-menu-smoke)
set_tests_properties(goldendict_search_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/search-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/search-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_edit_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --edit-menu-smoke)
set_tests_properties(goldendict_edit_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/edit-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/edit-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_history_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-preferences-smoke)
set_tests_properties(goldendict_history_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_favorites_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --favorites-preferences-smoke)
set_tests_properties(goldendict_favorites_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_articles_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --articles-preferences-smoke)
set_tests_properties(goldendict_articles_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/articles-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/articles-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_synonym_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --synonym-preferences-smoke)
set_tests_properties(goldendict_synonym_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/synonym-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/synonym-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_optional_parts_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --optional-parts-preferences-smoke)
set_tests_properties(goldendict_optional_parts_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/optional-parts-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/optional-parts-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_hide_single_tab_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --hide-single-tab-preferences-smoke)
set_tests_properties(goldendict_hide_single_tab_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/hide-single-tab-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/hide-single-tab-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_mru_tab_order_preferences_smoke
  COMMAND $<TARGET_FILE:goldendict> --mru-tab-order-preferences-smoke)
set_tests_properties(goldendict_mru_tab_order_preferences_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/mru-tab-order-preferences-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/mru-tab-order-preferences-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_escape_hides_main_window_preferences_smoke
  COMMAND ${CMAKE_COMMAND}
    -DGOLDENDICT_EXECUTABLE=$<TARGET_FILE:goldendict>
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/escape-preferences-test-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/escape_hides_main_window_restart.cmake)
set_tests_properties(goldendict_escape_hides_main_window_preferences_smoke
  PROPERTIES TIMEOUT 40)

add_test(NAME goldendict_article_click_preferences_smoke
  COMMAND ${CMAKE_COMMAND}
    -DGOLDENDICT_EXECUTABLE=$<TARGET_FILE:goldendict>
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-click-test-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/article_click_restart.cmake)
set_tests_properties(goldendict_article_click_preferences_smoke
  PROPERTIES TIMEOUT 50)

add_test(NAME goldendict_proxy_preferences_smoke
  COMMAND ${CMAKE_COMMAND}
    -DGOLDENDICT_EXECUTABLE=$<TARGET_FILE:goldendict>
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/proxy-preferences-test-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/proxy_preferences_restart.cmake)
set_tests_properties(goldendict_proxy_preferences_smoke PROPERTIES TIMEOUT 40)

add_test(NAME goldendict_network_cache_preferences_smoke
  COMMAND ${CMAKE_COMMAND}
    -DGOLDENDICT_EXECUTABLE=$<TARGET_FILE:goldendict>
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/network-cache-preferences-test-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/network_cache_preferences_restart.cmake)
set_tests_properties(goldendict_network_cache_preferences_smoke
  PROPERTIES TIMEOUT 40)

add_test(NAME goldendict_view_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --view-menu-smoke)
set_tests_properties(goldendict_view_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/view-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/view-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_history_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-menu-smoke)
set_tests_properties(goldendict_history_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_favorites_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --favorites-menu-smoke)
set_tests_properties(goldendict_favorites_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_help_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --help-menu-smoke)
set_tests_properties(goldendict_help_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/help-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/help-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_webengine_smoke
  COMMAND $<TARGET_FILE:goldendict> --webengine-smoke)
set_tests_properties(goldendict_webengine_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_webengine_interaction_smoke
  COMMAND $<TARGET_FILE:goldendict> --webengine-interaction-smoke)
set_tests_properties(goldendict_webengine_interaction_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_article_context_menu_smoke
  COMMAND $<TARGET_FILE:goldendict> --article-context-menu-smoke)
set_tests_properties(goldendict_article_context_menu_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-context-menu-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-context-menu-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_dictionary_context_navigation_smoke
  COMMAND $<TARGET_FILE:goldendict> --dictionary-context-navigation-smoke)
set_tests_properties(goldendict_dictionary_context_navigation_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-context-navigation-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-context-navigation-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_system_print_smoke
  COMMAND $<TARGET_FILE:goldendict> --system-print-smoke)
set_tests_properties(goldendict_system_print_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/system-print-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/system-print-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_article_tabs_smoke
  COMMAND $<TARGET_FILE:goldendict> --article-tabs-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_article_tabs_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-tabs-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-tabs-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_suggestion_pane_smoke
  COMMAND $<TARGET_FILE:goldendict> --suggestion-pane-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_suggestion_pane_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/suggestion-pane-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/suggestion-pane-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_dictionary_bar_smoke
  COMMAND $<TARGET_FILE:goldendict> --dictionary-bar-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_dictionary_bar_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-bar-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-bar-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_article_tab_session_restart_smoke
  COMMAND ${CMAKE_COMMAND}
    -DGOLDENDICT_EXECUTABLE=$<TARGET_FILE:goldendict>
    -DDICTIONARY_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-tab-session-restart-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/article_tab_session_restart.cmake)
set_tests_properties(goldendict_article_tab_session_restart_smoke PROPERTIES
  TIMEOUT 40)

add_test(NAME goldendict_history_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-smoke)
set_tests_properties(goldendict_history_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_dictionary_groups_smoke
  COMMAND $<TARGET_FILE:goldendict> --dictionary-groups-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_dictionary_groups_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-groups-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-groups-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_source_directories_smoke
  COMMAND $<TARGET_FILE:goldendict> --source-directories-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_source_directories_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/source-directories-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/source-directories-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_favorites_smoke
  COMMAND $<TARGET_FILE:goldendict> --favorites-smoke)
set_tests_properties(goldendict_favorites_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-management-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-management-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_favorites_transfer_smoke
  COMMAND $<TARGET_FILE:goldendict> --favorites-transfer-smoke)
set_tests_properties(goldendict_favorites_transfer_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-transfer-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-transfer-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_favorites_cross_folder_move_smoke
  COMMAND $<TARGET_FILE:goldendict> --favorites-cross-folder-move-smoke)
set_tests_properties(goldendict_favorites_cross_folder_move_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-cross-folder-move-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/favorites-cross-folder-move-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_dictionary_browser_smoke
  COMMAND $<TARGET_FILE:goldendict> --dictionary-browser-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_dictionary_browser_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-browser-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-browser-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_dictionary_browser_export_smoke
  COMMAND $<TARGET_FILE:goldendict> --dictionary-browser-export-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_dictionary_browser_export_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-browser-export-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/dictionary-browser-export-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_history_management_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-management-smoke)
set_tests_properties(goldendict_history_management_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-management-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-management-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_history_export_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-export-smoke)
set_tests_properties(goldendict_history_export_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-export-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-export-test-home/cache"
  TIMEOUT 20)

add_test(NAME goldendict_history_import_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-import-smoke)
set_tests_properties(goldendict_history_import_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-import-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-import-test-home/cache"
  TIMEOUT 20)
