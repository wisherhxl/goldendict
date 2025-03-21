# Standalone Tiger plugins build scripts
#
# Useful Tiger common build variables:
# - CMAKE_BUILD_TYPE=Release/Debug
# - BUILD_WITH_DEBUG_INFO=ON
# - ENABLE_BUILD_HARDENING=ON
#
# Plugin configuration variables:
# - TIGER_PLUGIN_DEPS - set of extra dependencies (modules), used for include dirs, target_link_libraries
# - TIGER_PLUGIN_SUFFIX
# - TIGER_PLUGIN_NAME
# - TIGER_PLUGIN_OUTPUT_NAME_FULL (overrides both TIGER_PLUGIN_NAME / TIGER_PLUGIN_SUFFIX)
#
#=============================================

if(NOT ${PROJECT_NAME}_SOURCE_DIR)
  message(FATAL_ERROR "${PROJECT_NAME}_SOURCE_DIR must be set to build the plugin!")
endif()

if(NOT DEFINED CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()
message(STATUS "CMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

set(BUILD_SHARED_LIBS ON CACHE BOOL "")
if(NOT BUILD_SHARED_LIBS)
  message(FATAL_ERROR "Static plugin build does not make sense")
endif()

# re-use Tiger build scripts
include("${${PROJECT_NAME}_SOURCE_DIR}/cmake/TigerUtils.cmake")
include("${${PROJECT_NAME}_SOURCE_DIR}/cmake/TigerDetectCXXCompiler.cmake")
include("${${PROJECT_NAME}_SOURCE_DIR}/cmake/TigerCompilerOptions.cmake")

function(ti_create_plugin module default_name dependency_target dependency_target_desc)

  set(TIGER_PLUGIN_NAME ${default_name} CACHE STRING "")
  set(TIGER_PLUGIN_DESTINATION "" CACHE PATH "")
  project(${TIGER_PLUGIN_NAME} LANGUAGES CXX)

  if(NOT TARGET ${dependency_target})
    message(FATAL_ERROR "${dependency_target_desc} was not found! (missing target ${dependency_target})")
  endif()

  set(modules_ROOT "${${PROJECT_NAME}_SOURCE_DIR}/modules")
  set(module_ROOT "${modules_ROOT}/${module}")

  foreach(src ${ARGN})
    list(APPEND sources "${module_ROOT}/${src}")
  endforeach()

  add_library(${TIGER_PLUGIN_NAME} MODULE
      "${sources}"
      ${TIGER_PLUGIN_EXTRA_SRC_FILES}
  )

  if(TIGER_PLUGIN_DEPS)
    foreach(d ${TIGER_PLUGIN_DEPS})
      list(APPEND TIGER_PLUGIN_EXTRA_INCLUDES "${modules_ROOT}/${d}/include")
    endforeach()
  endif()

  target_include_directories(${TIGER_PLUGIN_NAME} PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
      "${module_ROOT}/src"
      "${module_ROOT}/include"
      ${TIGER_PLUGIN_EXTRA_INCLUDES}
  )
  target_compile_definitions(${TIGER_PLUGIN_NAME} PRIVATE "BUILD_PLUGIN=1")

  target_link_libraries(${TIGER_PLUGIN_NAME} PRIVATE ${dependency_target})
  set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_VISIBILITY_PRESET hidden
  )

  if(DEFINED TIGER_PLUGIN_MODULE_PREFIX)
    set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES PREFIX "${TIGER_PLUGIN_MODULE_PREFIX}")
  endif()

  if(WIN32 OR NOT APPLE)
    set(TIGER_PLUGIN_NO_LINK FALSE CACHE BOOL "")
  else()
    set(TIGER_PLUGIN_NO_LINK TRUE CACHE BOOL "")
  endif()

  if(TIGER_PLUGIN_NO_LINK)
    if(APPLE)
      set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES LINK_FLAGS "-undefined dynamic_lookup")
    endif()
  else()
    find_package(Tiger REQUIRED ${module} ${TIGER_PLUGIN_DEPS})
    target_link_libraries(${TIGER_PLUGIN_NAME} PRIVATE ${${PROJECT_NAME}_LIBRARIES})
  endif()

  if(NOT ${PROJECT_NAME}_FOUND)  # build against sources (Linux)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/opencv2/opencv_modules.hpp" "#pragma once")
  endif()

  if(WIN32)
    ti_update(TIGER_DEBUG_POSTFIX d)
  endif()
  set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}")

  if(DEFINED TIGER_PLUGIN_SUFFIX)
    # custom value
  else()
    if(WIN32)
      ti_update(TIGER_PLUGIN_VERSION "${${PROJECT_NAME}_VERSION_MAJOR}${${PROJECT_NAME}_VERSION_MINOR}${${PROJECT_NAME}_VERSION_PATCH}")
      if(CMAKE_CXX_SIZEOF_DATA_PTR EQUAL 8)
        ti_update(TIGER_PLUGIN_ARCH "_64")
      else()
        ti_update(TIGER_PLUGIN_ARCH "")
      endif()
    else()
      # empty
    endif()
    ti_update(TIGER_PLUGIN_SUFFIX "${TIGER_PLUGIN_VERSION}${TIGER_PLUGIN_ARCH}")
  endif()

  if(TIGER_PLUGIN_DESTINATION)
    set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${TIGER_PLUGIN_DESTINATION}")
    message(STATUS "Output destination: ${TIGER_PLUGIN_DESTINATION}")
  endif()

  if(TIGER_PLUGIN_OUTPUT_NAME_FULL)
    set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES OUTPUT_NAME "${TIGER_PLUGIN_OUTPUT_NAME_FULL}")
  elseif(TIGER_PLUGIN_OUTPUT_NAME)
    set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES OUTPUT_NAME "${TIGER_PLUGIN_OUTPUT_NAME}${TIGER_PLUGIN_SUFFIX}")
  else()
    set_target_properties(${TIGER_PLUGIN_NAME} PROPERTIES OUTPUT_NAME "${TIGER_PLUGIN_NAME}${TIGER_PLUGIN_SUFFIX}")
  endif()

  install(TARGETS ${TIGER_PLUGIN_NAME} LIBRARY DESTINATION . COMPONENT plugins)

  message(STATUS "Library name: ${TIGER_PLUGIN_NAME}")

endfunction()
