foreach(mode clean source interface-source dependency genex)
  execute_process(COMMAND "${CMAKE_COMMAND}" -S "${FIXTURE}" -B "${OUTPUT}/${mode}" -G "${GENERATOR}"
    "-DCMAKE_MAKE_PROGRAM=${MAKE_PROGRAM}"
    "-DGUARD=${GUARD}" "-DMODE=${mode}"
    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
  file(WRITE "${OUTPUT}/${mode}.log" "${out}\n${err}\nExit: ${result}\n")
  if(mode STREQUAL "clean")
    if(NOT result EQUAL 0)
      message(FATAL_ERROR "Clean target probe failed: ${out}${err}")
    endif()
  elseif(result EQUAL 0 OR NOT "${out}${err}" MATCHES "W3.1 isolation violation")
    message(FATAL_ERROR "${mode} probe did not reject actual target violation: ${out}${err}")
  endif()
  message(STATUS "${mode}: expected result observed (${result})")
endforeach()
