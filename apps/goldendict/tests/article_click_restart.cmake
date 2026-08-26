# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${TEST_HOME}/home" "${TEST_HOME}/config" "${TEST_HOME}/cache")

set(test_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${TEST_HOME}/home"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --article-click-preferences-smoke
  RESULT_VARIABLE first_result)
if(NOT first_result EQUAL 0)
  message(FATAL_ERROR "Article click preferences smoke failed: ${first_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --article-click-restart-smoke
  RESULT_VARIABLE restart_result)
if(NOT restart_result EQUAL 0)
  message(FATAL_ERROR "Article click restart smoke failed: ${restart_result}")
endif()
