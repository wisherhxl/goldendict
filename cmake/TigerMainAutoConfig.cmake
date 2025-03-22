set(TI_BASE_MODULE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/modules/base)
set(TI_BASE_INCLUDE_DIR ${TI_BASE_MODULE_DIR}/include/${TI_INTERNAL_NAME})
set(TI_BASE_HEADER_DIR ${TI_BASE_INCLUDE_DIR}/base)
ti_create_directory(${TI_BASE_HEADER_DIR})
set(TI_BASE_SOURCE_DIR ${TI_BASE_MODULE_DIR}/src)
ti_create_directory(${TI_BASE_SOURCE_DIR})

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_base_cmake_lists.txt.in
  ${TI_BASE_MODULE_DIR}/CMakeLists.txt @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/project_config.h.in
               ${TI_BASE_HEADER_DIR}/project_config.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_def.h.in
               ${TI_BASE_HEADER_DIR}/ti_def.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/types_c.h.in
               ${TI_BASE_HEADER_DIR}/types_c.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/types_c.cpp.in
               ${TI_BASE_SOURCE_DIR}/types_c.cpp @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_version.h.in
               ${TI_BASE_HEADER_DIR}/ti_version.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_cpu_dispatch.h.in
               ${TI_BASE_HEADER_DIR}/ti_cpu_dispatch.h @ONLY)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/ti_base.h.in
               ${TI_BASE_INCLUDE_DIR}/base.h @ONLY)
