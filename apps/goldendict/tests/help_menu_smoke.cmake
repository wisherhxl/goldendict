# SPDX-License-Identifier: GPL-3.0-or-later

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY "${TEST_HOME}/config" "${TEST_HOME}/cache")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "XDG_SESSION_TYPE=x11"
    "QT_QPA_PLATFORM=offscreen"
    "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
    "LANG=en_US.UTF-8"
    "XDG_CONFIG_HOME=${TEST_HOME}/config"
    "XDG_CACHE_HOME=${TEST_HOME}/cache"
    "${GOLDENDICT_EXECUTABLE}" --help-menu-smoke "${HELP_DIRECTORY}"
  RESULT_VARIABLE help_menu_result)
if(NOT help_menu_result EQUAL 0)
  message(FATAL_ERROR "Help menu smoke failed: ${help_menu_result}")
endif()
