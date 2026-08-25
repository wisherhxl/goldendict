# SPDX-License-Identifier: GPL-3.0-or-later

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${TEST_HOME}/preferences/config"
  "${TEST_HOME}/preferences/cache"
  "${TEST_HOME}/fallback/config"
  "${TEST_HOME}/fallback/cache")

set(common_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${common_environment}
    "LANG=en_US.UTF-8"
    "XDG_CONFIG_HOME=${TEST_HOME}/preferences/config"
    "XDG_CACHE_HOME=${TEST_HOME}/preferences/cache"
    "${GOLDENDICT_EXECUTABLE}" --help-menu-smoke "${HELP_DIRECTORY}"
  RESULT_VARIABLE preferences_result)
if(NOT preferences_result EQUAL 0)
  message(FATAL_ERROR
    "Interface language Preferences smoke failed: ${preferences_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${common_environment}
    "LANG=en_US.UTF-8"
    "XDG_CONFIG_HOME=${TEST_HOME}/preferences/config"
    "XDG_CACHE_HOME=${TEST_HOME}/preferences/cache"
    "${GOLDENDICT_EXECUTABLE}" --interface-language-startup-smoke
  RESULT_VARIABLE russian_restart_result)
if(NOT russian_restart_result EQUAL 0)
  message(FATAL_ERROR
    "Russian interface restart smoke failed: ${russian_restart_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${common_environment}
    "LANG=fr_FR.UTF-8"
    "XDG_CONFIG_HOME=${TEST_HOME}/fallback/config"
    "XDG_CACHE_HOME=${TEST_HOME}/fallback/cache"
    "${GOLDENDICT_EXECUTABLE}" --interface-language-startup-smoke
  RESULT_VARIABLE unmatched_result)
if(NOT unmatched_result EQUAL 0)
  message(FATAL_ERROR
    "Unmatched locale fallback smoke failed: ${unmatched_result}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${common_environment}
    "LANG=ru_RU.UTF-8"
    "XDG_CONFIG_HOME=${TEST_HOME}/fallback/config"
    "XDG_CACHE_HOME=${TEST_HOME}/fallback/cache"
    "${GOLDENDICT_EXECUTABLE}" --interface-language-unsupported-smoke
  RESULT_VARIABLE unsupported_result)
if(NOT unsupported_result EQUAL 0)
  message(FATAL_ERROR
    "Unsupported explicit locale fallback smoke failed: ${unsupported_result}")
endif()
