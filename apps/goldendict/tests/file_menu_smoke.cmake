# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${TEST_HOME}/home" "${TEST_HOME}/config" "${TEST_HOME}/cache")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "XDG_SESSION_TYPE=x11"
    "QT_QPA_PLATFORM=offscreen"
    "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
    "HOME=${TEST_HOME}/home"
    "XDG_CONFIG_HOME=${TEST_HOME}/config"
    "XDG_CACHE_HOME=${TEST_HOME}/cache"
    "${GOLDENDICT_EXECUTABLE}" --file-menu-smoke
  RESULT_VARIABLE file_menu_result)
if(NOT file_menu_result EQUAL 0)
  message(FATAL_ERROR "File menu smoke failed: ${file_menu_result}")
endif()
