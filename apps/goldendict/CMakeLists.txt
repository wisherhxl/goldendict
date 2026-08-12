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

add_test(NAME goldendict_history_smoke
  COMMAND $<TARGET_FILE:goldendict> --history-smoke)
set_tests_properties(goldendict_history_smoke PROPERTIES
  ENVIRONMENT
    "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-test-home/config;XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/history-test-home/cache"
  TIMEOUT 20)
