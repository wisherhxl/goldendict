if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()
if(NOT IS_ABSOLUTE "${TEST_HOME}" OR NOT TEST_HOME MATCHES "/view-prefs-test-home$")
  message(FATAL_ERROR "Unexpected TEST_HOME: ${TEST_HOME}")
endif()
file(REMOVE_RECURSE "${TEST_HOME}")
file(MAKE_DIRECTORY "${TEST_HOME}/home" "${TEST_HOME}/config" "${TEST_HOME}/cache")
if(NOT DEFINED SMOKE_PLATFORM)
  if(CMAKE_HOST_WIN32)
    # Windows WebEngine runtime recomposition needs a native graphics surface.
    set(SMOKE_PLATFORM windows)
  else()
    set(SMOKE_PLATFORM offscreen)
  endif()
endif()
set(smoke_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=${SMOKE_PLATFORM}"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${TEST_HOME}/home"
  "XDG_CONFIG_HOME=${TEST_HOME}/config"
  "XDG_CACHE_HOME=${TEST_HOME}/cache"
  "GOLDENDICT_TEST_CONFIG_ROOT=${TEST_HOME}/windows-config")

foreach(step read-default write-enabled read-enabled write-default read-default)
  if(step MATCHES "^write")
    set(phase write)
  else()
    set(phase read)
  endif()
  if(step MATCHES "enabled$")
    set(enabled 1)
  else()
    set(enabled 0)
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env ${smoke_environment}
      "GOLDENDICT_VIEW_RESTART_PHASE=${phase}"
      "GOLDENDICT_VIEW_RESTART_ENABLED=${enabled}"
      "${GOLDENDICT_EXECUTABLE}" --view-preferences-restart-smoke -style Fusion
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 15)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "View restart ${step} failed (${result})\n${output}\n${error}")
  endif()
endforeach()
