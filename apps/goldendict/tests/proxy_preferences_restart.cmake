# SPDX-License-Identifier: GPL-3.0-or-later

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY "${TEST_HOME}/config" "${TEST_HOME}/cache")
set(test_environment
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --proxy-preferences-smoke
  RESULT_VARIABLE first_result)
if(NOT first_result EQUAL 0)
  message(FATAL_ERROR "Proxy Preferences smoke failed: ${first_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --proxy-preferences-restart-smoke
  RESULT_VARIABLE restart_result)
if(NOT restart_result EQUAL 0)
  message(FATAL_ERROR "Proxy Preferences restart failed: ${restart_result}")
endif()
