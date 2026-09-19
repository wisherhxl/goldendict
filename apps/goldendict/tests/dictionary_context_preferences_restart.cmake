# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED TEST_EXECUTABLE OR NOT EXISTS "${TEST_EXECUTABLE}")
  message(FATAL_ERROR "TEST_EXECUTABLE is required")
endif()
if(NOT DEFINED TEST_HOME)
  if(DEFINED ENV{TEMP} AND NOT "$ENV{TEMP}" STREQUAL "")
    set(TEST_HOME "$ENV{TEMP}")
  elseif(DEFINED ENV{TMPDIR} AND NOT "$ENV{TMPDIR}" STREQUAL "")
    set(TEST_HOME "$ENV{TMPDIR}")
  else()
    set(TEST_HOME "${CMAKE_CURRENT_BINARY_DIR}/dictionary-context-preferences-test-home")
  endif()
endif()

if(NOT IS_ABSOLUTE "${TEST_HOME}")
  message(FATAL_ERROR "TEST_HOME must be an absolute owned temporary location")
endif()
string(RANDOM LENGTH 6 ALPHABET 0123456789abcdef run_id)
set(run_home "${TEST_HOME}/dc-${run_id}")
if(EXISTS "${run_home}")
  message(FATAL_ERROR "Refusing to reuse an existing run directory")
endif()
file(MAKE_DIRECTORY
  "${run_home}/home" "${run_home}/config" "${run_home}/cache" "${run_home}/tmp")

set(test_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${run_home}/home"
  "XDG_CONFIG_HOME=${run_home}/config"
  "XDG_CACHE_HOME=${run_home}/cache"
  "TEMP=${run_home}/tmp" "TMP=${run_home}/tmp"
  "GOLDENDICT_CONTEXT_RESTART_ROOT=${run_home}")

foreach(run RANGE 1 2)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
      "GOLDENDICT_CONTEXT_RESTART_PASS=${run}"
      "${TEST_EXECUTABLE}" -o "${run_home}/pass-${run}.txt,txt"
    RESULT_VARIABLE result)
  if(EXISTS "${run_home}/pass-${run}.txt")
    file(READ "${run_home}/pass-${run}.txt" output)
    message(STATUS "Restart pass ${run}: ${output}")
  endif()
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "Dictionary context preferences run ${run} failed: ${result}")
  endif()
  if(NOT EXISTS "${run_home}/profile/current-config/core.conf")
    message(FATAL_ERROR "Restart pass ${run} did not persist the shared owned profile")
  endif()
endforeach()
message(STATUS "Two independent processes verified shared profile: ${run_home}")
