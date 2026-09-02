# SPDX-License-Identifier: GPL-3.0-or-later

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${TEST_HOME}/home" "${TEST_HOME}/config" "${TEST_HOME}/cache"
  "${TEST_HOME}/appdata" "${TEST_HOME}/localappdata"
  "${TEST_HOME}/windows-config")
set(test_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${TEST_HOME}/home"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache"
  "APPDATA=${TEST_HOME}/appdata"
  "LOCALAPPDATA=${TEST_HOME}/localappdata")

set(owned_cache_root "${TEST_HOME}/cache")
set(windows_smoke_root "")
if(WIN32)
  string(SHA256 smoke_identity "${TEST_HOME}")
  string(SUBSTRING "${smoke_identity}" 0 12 smoke_identity)
  file(TO_CMAKE_PATH "$ENV{TEMP}" windows_temp_root)
  set(windows_smoke_root
    "${windows_temp_root}/goldendict-smoke-${smoke_identity}")
  if(NOT windows_smoke_root MATCHES
      "/goldendict-smoke-[0-9a-f]+$")
    message(FATAL_ERROR "Unsafe Windows smoke root: ${windows_smoke_root}")
  endif()
  file(REMOVE_RECURSE "${windows_smoke_root}")
  file(MAKE_DIRECTORY "${windows_smoke_root}")
  list(APPEND test_environment
    "GOLDENDICT_TEST_CONFIG_ROOT=${windows_smoke_root}")
  set(owned_cache_root "${windows_smoke_root}/cache")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
    "${GOLDENDICT_EXECUTABLE}" --network-cache-preferences-smoke
  RESULT_VARIABLE first_result)
if(NOT first_result EQUAL 0)
  message(FATAL_ERROR "Network cache Preferences smoke failed: ${first_result}")
endif()

set(owned_cache_pattern "${owned_cache_root}/*/*/qt-network-http")
if(WIN32)
  set(owned_cache_pattern "${owned_cache_root}/qt-network-http")
endif()
file(GLOB owned_cache_candidates LIST_DIRECTORIES true
  "${owned_cache_pattern}")
list(LENGTH owned_cache_candidates owned_cache_count)
if(NOT owned_cache_count EQUAL 1)
  message(FATAL_ERROR "Owned Qt Network cache directory was not retained")
endif()
list(GET owned_cache_candidates 0 owned_cache)
file(WRITE "${owned_cache_root}/webengine-sentinel" "browser")

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
if(NOT EXISTS "${owned_cache_root}/webengine-sentinel")
  message(FATAL_ERROR "Cache cleanup escaped the owned Qt Network directory")
endif()
if(windows_smoke_root)
  file(REMOVE_RECURSE "${windows_smoke_root}")
endif()
