# SPDX-License-Identifier: GPL-3.0-or-later
# Exercise the registered real runners, including their custom event-loop entry.
if(NOT EXISTS "${TEST_BUILD_DIRECTORY}/CTestTestfile.cmake" OR
   NOT EXISTS "${CTEST_COMMAND}")
  message(FATAL_ERROR "Configured CTest build and executable are required")
endif()
if(NOT IS_ABSOLUTE "$ENV{TEMP}")
  message(FATAL_ERROR "An absolute temporary directory is required")
endif()
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef run_id)
file(TO_CMAKE_PATH "$ENV{TEMP}" temporary_root)
set(root "${temporary_root}/ep-${run_id}")
if(EXISTS "${root}")
  message(FATAL_ERROR "Refusing to reuse execution-chain data")
endif()
file(MAKE_DIRECTORY "${root}")
set(invalid FALSE)
foreach(family optional_parts_preferences history_import history_preferences history_smoke)
  string(REPLACE "_" "-" report_name "${family}")
  foreach(fail RANGE 0 1)
    set(owned "${root}/${family}-${fail}")
    file(MAKE_DIRECTORY "${owned}/tmp")
    set(case_name "goldendict_${family}_smoke")
    if(family STREQUAL "history_smoke")
      set(case_name "goldendict_history_smoke")
    endif()
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E env
        "TEMP=${owned}/tmp" "TMP=${owned}/tmp"
        "GOLDENDICT_TEST_EXPECT_FAILURE=${fail}"
        "${CTEST_COMMAND}" --test-dir "${TEST_BUILD_DIRECTORY}"
        -R "^${case_name}$" -V
      RESULT_VARIABLE child_result OUTPUT_VARIABLE out ERROR_VARIABLE err
      TIMEOUT 30)
    file(WRITE "${owned}/ctest.txt" "${out}\n${err}\nCTest exit: ${child_result}\n")
    set(report "${REPORT_DIRECTORY}/${report_name}-test.txt")
    if(NOT EXISTS "${report}")
      message(FATAL_ERROR "Missing actual QtTest report: ${report}")
    endif()
    file(READ "${report}" qt_report)
    file(WRITE "${owned}/qtest.txt" "${qt_report}")
    set(valid TRUE)
    if(NOT "${out}" MATCHES "1/1 Test" OR
       NOT "${qt_report}" MATCHES "${owned}/tmp" OR
       NOT "${qt_report}" MATCHES "Execution-chain resource cleanup complete" OR
       NOT "${qt_report}" MATCHES "PASS[ ]*: .*::cleanupTestCase")
      set(valid FALSE)
    endif()
    if(fail EQUAL 0)
      if(NOT child_result EQUAL 0 OR
         NOT "${out}" MATCHES "Execution-chain results: qtest=0 event_loop=0 process=0" OR
         NOT "${qt_report}" MATCHES "Totals: [1-9][0-9]* passed, 0 failed, 0 skipped")
        set(valid FALSE)
      endif()
    else()
      if(NOT child_result EQUAL 8 OR
         NOT "${out}" MATCHES "Execution-chain results: qtest=1 event_loop=[0-9]+ process=1" OR
         NOT "${qt_report}" MATCHES "controlled execution-chain assertion failure" OR
         NOT "${qt_report}" MATCHES "Totals: [0-9]+ passed, 1 failed, 0 skipped")
        set(valid FALSE)
      endif()
    endif()
    file(WRITE "${owned}/result.txt" "Expected failure: ${fail}\nCTest exit: ${child_result}\nChecker accepts: ${valid}\n")
    message(STATUS "${family}/expected-failure=${fail}: CTest=${child_result}; verified=${valid}; evidence=${owned}")
    if(NOT valid)
      set(invalid TRUE)
    endif()
  endforeach()
endforeach()
if(invalid)
  message(FATAL_ERROR "Execution-chain result was not preserved; inspect ${root}")
endif()
message(STATUS "Execution-chain success and expected-failure checks passed: ${root}")
