foreach(family full_text_dictionary_scope_test dictionary_bar_test dictionary_scope_test_access)
foreach(mode clean source interface-source dependency genex)
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${OUTPUT}/${family}-${mode}" -G "${GENERATOR}"
    "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
    "-DFAMILY=${family}" "-DGUARD=${GUARD}" "-DMODE=${mode}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
  file(WRITE "${OUTPUT}/${family}-${mode}.log" "${out}\n${err}\nExit: ${result}\n")
  if(mode STREQUAL "clean")
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "Clean target probe failed: ${out}${err}")
    endif()
  elseif(result EQUAL 0 OR NOT "${out}${err}" MATCHES "W3 isolation violation")
    message(FATAL_ERROR "${mode} probe did not reject actual target violation: ${out}${err}")
  endif()
  message(STATUS "${family}/${mode}: expected result observed (${result})")
endforeach()

endforeach()
