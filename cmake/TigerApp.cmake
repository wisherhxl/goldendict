# ensures that all passed third party dependencies are available sets
# TI_EXTRA_DEPENDENCIES_FOUND variable to TRUE/FALSE
macro(ti_check_extra_dependencies)
  set(TI_EXTRA_DEPENDENCIES_FOUND TRUE)
  foreach(d ${ARGN})
    if(NOT TARGET ${d})
      message(FATAL_ERROR "Cannot find 3rd-party dependency: ${d}")
      set(TI_EXTRA_DEPENDENCIES_FOUND FALSE)
      break()
    endif()
  endforeach()
endmacro()

# glob files to compile source files are set to variable ${the_target}_SRC
# include directories are set to variable ${the_target}_SRC_INCLUDE qt
# translation files are created
macro(ti_glob_app_source the_target)
  set(${the_target}_SRC "")
  set(${the_target}_SRC_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/src/)

  set(PROJ_SRC_UI "")
  set(PROJ_SRC_H "")
  set(PROJ_SRC_CPP "")

  file(
    GLOB_RECURSE src_ui
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.ui")
  foreach(src ${src_ui})
    set(src_path_absolute ${CMAKE_CURRENT_SOURCE_DIR}/src/${src})
    get_filename_component(src_path "${src}" PATH)
    string(REPLACE "/" "\\" src_path_msvc "${src_path}")
    list(APPEND PROJ_SRC_UI ${src_path_absolute})
    source_group("${src_path_msvc}" FILES "${src_path_absolute}")
  endforeach()

  file(
    GLOB_RECURSE src_h
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
  foreach(src ${src_h})
    set(src_path_absolute ${CMAKE_CURRENT_SOURCE_DIR}/src/${src})
    get_filename_component(src_path "${src}" PATH)
    string(REPLACE "/" "\\" src_path_msvc "${src_path}")
    list(APPEND PROJ_SRC_H ${src_path_absolute})
    source_group("${src_path_msvc}" FILES "${src_path_absolute}")
  endforeach()

  file(
    GLOB_RECURSE src_cpp
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.rc")
  foreach(src ${src_cpp})
    set(src_path_absolute ${CMAKE_CURRENT_SOURCE_DIR}/src/${src})
    get_filename_component(src_path "${src}" PATH)
    string(REPLACE "/" "\\" src_path_msvc "${src_path}")
    list(APPEND PROJ_SRC_CPP ${src_path_absolute})
    source_group("${src_path_msvc}" FILES "${src_path_absolute}")
  endforeach()

  qt6_create_translation(QM_FILES ${PROJ_SRC_CPP} ${PROJ_SRC_UI} lang/zh_cn.ts)

  set(${the_target}_SRC ${PROJ_SRC_H} ${PROJ_SRC_CPP} ${PROJ_SRC_UI}
                        ${QM_FILES})
endmacro()

# Unified function for creating Tiger applications: ti_add_app(tgt [MODULES <m1>
# [<m2> ...]] [EXTRAS <e1> [<e2> ...]]) MODULES are dependencies of the Tiger
# module EXTRAS are dependencies of the 3rd-party libries
macro(ti_add_app the_target)
  cmake_parse_arguments(APP "" "" "MODULES;EXTRAS" ${ARGN})

  set(__app_modules "")
  foreach(mod ${APP_MODULES})
    list(APPEND __app_modules ${TI_INTERNAL_NAME}_${mod})
  endforeach()

  ti_check_dependencies(${__app_modules})
  if(NOT TI_DEPENDENCIES_FOUND)
    message(
      FATAL_ERROR "Module dependencies check failed for app: ${the_target}")
    return()
  endif()
  ti_check_extra_dependencies(${APP_EXTRAS})
  if(NOT TI_EXTRA_DEPENDENCIES_FOUND)
    message(
      FATAL_ERROR "Extra dependencies check failed for app: ${the_target}")
    return()
  endif()

  project(${the_target})
  ti_glob_app_source(${the_target})

  set(RES_QRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/resources/res.qrc)
  if(EXISTS ${RES_QRC_FILE})
    qt6_add_resources(${the_target}_SRC ${RES_QRC_FILE})
  endif()

  ti_add_executable(${the_target} ${${the_target}_SRC})

  target_include_directories(${the_target} PRIVATE ${${the_target}_SRC_INCLUDE})
  ti_target_include_modules_recurse(${the_target} ${__app_modules})
  ti_target_link_libraries(${the_target} ${__app_modules})
  target_link_libraries(${the_target} PRIVATE ${APP_EXTRAS})

  set_target_properties(
    ${the_target}
    PROPERTIES DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
               ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
               RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
               OUTPUT_NAME "${the_target}")

  set_target_properties(${the_target} PROPERTIES FOLDER "applications"
                                                 WIN32_EXECUTABLE TRUE)

  foreach(qm_file ${QM_FILES})
    add_custom_command(
      TARGET ${the_target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
              ${CMAKE_CURRENT_BINARY_DIR}/lang/)
    add_custom_command(
      TARGET ${the_target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${qm_file}
              ${CMAKE_CURRENT_BINARY_DIR}/lang)
  endforeach()

  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    set(DO_COPY_RES 0)
    if(NOT IS_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/config
       AND NOT IS_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/res)
      set(DO_COPY_RES 1)
    endif()
    set(${the_target}_COPY_CONFIG_DIRS
        ${DO_COPY_RES}
        CACHE BOOL "Copy resource directory to runtime")

    if(${${the_target}_COPY_CONFIG_DIRS})
      add_custom_command(
        TARGET ${the_target}
        POST_BUILD
        COMMAND
          ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/resources/ ${CMAKE_CURRENT_BINARY_DIR})
    endif()
  endif()

  install(TARGETS ${the_target} RUNTIME DESTINATION ${TIGER_BIN_INSTALL_PATH}
                                        COMPONENT dev)
endmacro()

# add applicationss from specified directories Usage:
# ti_add_app_from_directory(<location> [<extra location> ...])
macro(ti_add_app_from_directory)
  foreach(directory ${ARGN})
    if(BUILD_APPS_LIST)
      list(FIND BUILD_APPS_LIST ${directory} _index)
      if(${_index} GREATER -1)
        add_subdirectory(${directory})
      else()
        message(STATUS "Tiger Platform Skip app: ${directory}")
      endif()
    else()
      add_subdirectory(${directory})
    endif()
  endforeach()
endmacro()

# collect applicationss from specified directories NB: must be called only once!
# Usage: ti_glob_applications(<main location> [<extra location> ...])
macro(ti_glob_applications main_root)
  # support comma-separated list (,) too
  string(REPLACE "," ";" TIGER_INSTALL_APPS_LIST "${TIGER_INSTALL_APPS_LIST}")
  if(DEFINED TIGER_APP_INITIAL_PASS)
    message(
      FATAL_ERROR
        "Tiger has already loaded its modules. Calling ti_glob_applications second time is not
    allowed.")
  endif()
  set(TIGER_APP_INITIAL_PASS ON)

  # collect applications
  _glob_locations(__app_paths __app_names ${main_root})

  list(LENGTH __app_names __app_count)
  if(__app_count GREATER 0)
    _assert_uniqueness("Duplicated application LOCATIONS has been found"
                       ${__app_paths})
    _assert_uniqueness("Duplicated application NAMES has been found"
                       ${__app_names})
    ti_add_app_from_directory(${__app_names})
  endif()

endmacro()
