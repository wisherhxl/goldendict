if(NOT DEFINED GOLDENDICT_EXECUTABLE OR NOT DEFINED TEST_HOME)
  message(FATAL_ERROR "GOLDENDICT_EXECUTABLE and TEST_HOME are required")
endif()
get_filename_component(test_root "${TEST_HOME}" ABSOLUTE)
if(NOT test_root MATCHES "/inspector-geometry-test-home$")
  message(FATAL_ERROR "Unexpected test directory: ${test_root}")
endif()
# Use a fresh child without deleting a previous run's evidence.
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef run_id)
set(run_root "${test_root}/${run_id}")
file(MAKE_DIRECTORY "${run_root}")
if(NOT DEFINED TEST_QPA)
  set(TEST_QPA offscreen)
endif()
set(smoke_environment
  "XDG_SESSION_TYPE=x11"
  "QT_QPA_PLATFORM=${TEST_QPA}"
  "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
  "HOME=${run_root}/home"
  "XDG_CONFIG_HOME=${run_root}/config"
  "XDG_CACHE_HOME=${run_root}/cache"
  "APPDATA=${run_root}/appdata"
  "LOCALAPPDATA=${run_root}/localappdata"
  "GOLDENDICT_TEST_CONFIG_ROOT=${run_root}/windows-config")
foreach(mode inspector-geometry-smoke inspector-geometry-restart-smoke)
  execute_process(COMMAND "${CMAKE_COMMAND}" -E env ${smoke_environment}
    "${GOLDENDICT_EXECUTABLE}" "--${mode}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error
    TIMEOUT 20)
  file(WRITE "${run_root}/${mode}.log" "${output}\n${error}")
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "${mode} failed (${result})\n${output}\n${error}")
  endif()
endforeach()
