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
    "${GOLDENDICT_EXECUTABLE}" --network-cache-preferences-smoke
  RESULT_VARIABLE first_result)
if(NOT first_result EQUAL 0)
  message(FATAL_ERROR "Network cache Preferences smoke failed: ${first_result}")
endif()

file(GLOB owned_cache_candidates LIST_DIRECTORIES true
  "${TEST_HOME}/cache/*/*/qt-network-http")
list(LENGTH owned_cache_candidates owned_cache_count)
if(NOT owned_cache_count EQUAL 1)
  message(FATAL_ERROR "Owned Qt Network cache directory was not retained")
endif()
list(GET owned_cache_candidates 0 owned_cache)
file(WRITE "${TEST_HOME}/cache/webengine-sentinel" "browser")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --network-cache-preferences-restart-smoke
  RESULT_VARIABLE restart_result)
if(NOT restart_result EQUAL 0)
  message(FATAL_ERROR "Network cache Preferences restart failed: ${restart_result}")
endif()
if(EXISTS "${owned_cache}")
  message(FATAL_ERROR "Owned Qt Network cache directory was not cleared")
endif()
if(NOT EXISTS "${TEST_HOME}/cache/webengine-sentinel")
  message(FATAL_ERROR "Cache cleanup escaped the owned Qt Network directory")
endif()
