if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()
if(NOT TEST_HOME MATCHES "product-shell-test-home$")
  message(FATAL_ERROR "Refusing to clean unexpected TEST_HOME: ${TEST_HOME}")
endif()

file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${TEST_HOME}/home"
  "${TEST_HOME}/config"
  "${TEST_HOME}/cache"
  "${TEST_HOME}/appdata"
  "${TEST_HOME}/localappdata"
  "${TEST_HOME}/windows-config")

set(smoke_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=offscreen"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${TEST_HOME}/home"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache"
  "APPDATA=${TEST_HOME}/appdata"
  "LOCALAPPDATA=${TEST_HOME}/localappdata"
  "GOLDENDICT_TEST_CONFIG_ROOT=${TEST_HOME}/windows-config")

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env ${smoke_environment}
    "${GOLDENDICT_EXECUTABLE}" --product-shell-smoke
  RESULT_VARIABLE smoke_result
  OUTPUT_VARIABLE smoke_output
  ERROR_VARIABLE smoke_error)

if(NOT smoke_result EQUAL 0)
  message(FATAL_ERROR
    "Product shell smoke failed (${smoke_result})\n"
    "stdout:\n${smoke_output}\n"
    "stderr:\n${smoke_error}")
endif()
