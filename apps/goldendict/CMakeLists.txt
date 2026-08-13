# SPDX-License-Identifier: GPL-3.0-or-later

ti_add_qt_app(goldendict MODULES core EXTRAS ${qt_link})

add_test(NAME goldendict_smoke COMMAND $<TARGET_FILE:goldendict> --smoke)

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

add_test(NAME goldendict_article_tabs_smoke
  COMMAND $<TARGET_FILE:goldendict> --article-tabs-smoke
    --dictionary-root ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/dictd)
set_tests_properties(goldendict_article_tabs_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-tabs-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/article-tabs-test-home/cache"
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
