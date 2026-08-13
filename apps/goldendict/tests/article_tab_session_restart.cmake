# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED DICTIONARY_ROOT OR
   NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "Missing article-tab restart smoke argument")
endif()

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY "${TEST_HOME}/config" "${TEST_HOME}/cache")

set(smoke_environment
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache")

foreach(smoke_phase IN ITEMS prepare verify)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${smoke_environment}
      "${GOLDENDICT_EXECUTABLE}"
      "--article-tab-session-restart-${smoke_phase}"
      --dictionary-root "${DICTIONARY_ROOT}"
    RESULT_VARIABLE smoke_result
    COMMAND_ECHO STDOUT)
  if(NOT smoke_result EQUAL 0)
    message(FATAL_ERROR
      "Article-tab session restart ${smoke_phase} failed: ${smoke_result}")
  endif()
endforeach()
