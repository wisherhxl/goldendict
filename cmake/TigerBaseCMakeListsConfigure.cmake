if(TIGER_HAVE_PROTOS)
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_base_cmake_lists_proto.txt.in
    ${TI_BASE_MODULE_DIR}/CMakeLists.txt @ONLY)
else()
  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_base_cmake_lists.txt.in
    ${TI_BASE_MODULE_DIR}/CMakeLists.txt @ONLY)
endif()