# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY "${TEST_HOME}/config" "${TEST_HOME}/cache")

set(test_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache")

foreach(run RANGE 1 2)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${test_environment}
      "${GOLDENDICT_EXECUTABLE}" --dictionary-context-preferences-smoke
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "Dictionary context preferences run ${run} failed: ${result}")
  endif()
endforeach()
