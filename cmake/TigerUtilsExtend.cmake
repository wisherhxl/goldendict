# ##############################################################################
# short command to setup source group
function(ti_source_group group)
  if(BUILD_${TI_INTERNAL_NAME}_world
     AND TIGER_MODULE_${the_module}_IS_PART_OF_WORLD)
    set(group "${the_module}\\${group}")
  endif()
  cmake_parse_arguments(SG "" "DIRBASE" "GLOB;GLOB_RECURSE;FILES" ${ARGN})
  set(files "")
  if(SG_FILES)
    list(APPEND files ${SG_FILES})
  endif()
  if(SG_GLOB)
    file(GLOB srcs ${SG_GLOB})
    list(APPEND files ${srcs})
  endif()
  if(SG_GLOB_RECURSE)
    file(GLOB_RECURSE srcs ${SG_GLOB_RECURSE})
    list(APPEND files ${srcs})
  endif()
  if(SG_DIRBASE)
    foreach(f ${files})
      file(RELATIVE_PATH fpart "${SG_DIRBASE}" "${f}")
      if(fpart MATCHES "^\\.\\.")
        message(
          AUTHOR_WARNING
            "Can't detect subpath for source_group command: Group=${group} FILE=${f} DIRBASE=${SG_DIRBASE}"
        )
        set(fpart "")
      else()
        get_filename_component(fpart "${fpart}" PATH)
        if(fpart)
          set(fpart "/${fpart}") # add '/'
          string(REPLACE "/" "\\" fpart "${fpart}")
        endif()
      endif()
      source_group("${group}${fpart}" FILES ${f})
    endforeach()
  else()
    source_group(${group} FILES ${files})
  endif()
endfunction()

function(ti_target_link_libraries target)
  set(LINK_DEPS ${ARGN})
  _ti_fix_target(target)
  set(LINK_MODE "PRIVATE")
  set(LINK_PENDING "")
  foreach(dep ${LINK_DEPS})
    if(" ${dep}" STREQUAL " ${target}")
      # prevent "link to itself" warning (world problem)
    elseif(
      " ${dep}" STREQUAL " LINK_PRIVATE"
      OR " ${dep}" STREQUAL " LINK_PUBLIC" # deprecated
      OR " ${dep}" STREQUAL " PRIVATE"
      OR " ${dep}" STREQUAL " PUBLIC"
      OR " ${dep}" STREQUAL " INTERFACE")
      if(NOT LINK_PENDING STREQUAL "")
        __ti_push_target_link_libraries(${LINK_MODE} ${LINK_PENDING})
        set(LINK_PENDING "")
      endif()
      set(LINK_MODE "${dep}")
    else()
      if(BUILD_${TI_INTERNAL_NAME}_world)
        if(TIGER_MODULE_${dep}_IS_PART_OF_WORLD)
          set(dep ${TI_INTERNAL_NAME}_world)
        endif()
      endif()
      list(APPEND LINK_PENDING "${dep}")
    endif()
  endforeach()
  if(NOT LINK_PENDING STREQUAL "")
    __ti_push_target_link_libraries(${LINK_MODE} ${LINK_PENDING})
  endif()
endfunction()

macro(ti_get_libname var_name)
  get_filename_component(__libname "${ARGN}" NAME)
  # lib${TI_INTERNAL_NAME}_core.so.3.3 -> ${TI_INTERNAL_NAME}_core
  string(REGEX REPLACE "^lib(.+)\\.(a|so|dll)(\\.[.0-9]+)?$" "\\1" __libname
                       "${__libname}")
  # MacOSX: lib${TI_INTERNAL_NAME}_core.3.3.1.dylib -> ${TI_INTERNAL_NAME}_core
  string(REGEX REPLACE "^lib(.+[^.0-9])\\.([.0-9]+\\.)?dylib$" "\\1" __libname
                       "${__libname}")
  set(${var_name} "${__libname}")
endmacro()

# rename modules target to world if needed
macro(_ti_fix_target target_var)
  if(BUILD_${TI_INTERNAL_NAME}_world)
    if(TIGER_MODULE_${${target_var}}_IS_PART_OF_WORLD)
      set(${target_var} ${TI_INTERNAL_NAME}_world)
    endif()
  endif()
endmacro()

function(ti_is_tiger_directory result_var dir)
  set(result FALSE)
  foreach(parent ${${PROJECT_NAME}_SOURCE_DIR} ${${PROJECT_NAME}_BINARY_DIR}
                 ${TIGER_EXTRA_MODULES_PATH})
    ti_is_subdir(result "${parent}" "${dir}")
    if(result)
      break()
    endif()
  endforeach()
  set(${result_var}
      ${result}
      PARENT_SCOPE)
endfunction()

# adds include directories in such a way that directories from the Tiger source tree go first
function(ti_include_directories)
  ti_debug_message("ti_include_directories( ${ARGN} )")
  set(__add_before "")
  foreach(dir ${ARGN})
    ti_is_tiger_directory(__is_${TI_INTERNAL_NAME}_dir "${dir}")
    if(__is_${TI_INTERNAL_NAME}_dir)
      list(APPEND __add_before "${dir}")
    elseif(((TI_GCC AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6.0") OR TI_CLANG) AND
           dir MATCHES "/usr/include$")
      # workaround for GCC 6.x bug
    else()
      if(${CMAKE_SYSTEM_NAME} MATCHES QNX)
        include_directories(AFTER "${dir}")
      else()
        include_directories(AFTER SYSTEM "${dir}")
      endif()
    endif()
  endforeach()
  include_directories(BEFORE ${__add_before})
endfunction()

# adds include directories in such a way that directories from the Tiger source tree go first
function(ti_target_include_directories target)
  #ti_debug_message("ti_target_include_directories(${target} ${ARGN})")
  _ti_fix_target(target)
  set(__params "")
  set(__system_params "")
  set(__var_name __params)
  foreach(dir ${ARGN})
    if("${dir}" STREQUAL "SYSTEM")
      set(__var_name __system_params)
    else()
      if(TI_GCC AND NOT CMAKE_CXX_COMPILER_VERSION VERSION_LESS "6.0" AND
          dir MATCHES "/usr/include$")
         # workaround for GCC 6.x bug
      else()
        get_filename_component(__abs_dir "${dir}" ABSOLUTE)
        ti_is_tiger_directory(__is_${TI_INTERNAL_NAME}_dir "${dir}")
        if(__is_${TI_INTERNAL_NAME}_dir)
          list(APPEND ${__var_name} "${__abs_dir}")
        else()
          list(APPEND ${__var_name} "${dir}")
        endif()
      endif()
    endif()
  endforeach()
  if(HAVE_CUDA)
    include_directories(${__params})
    include_directories(SYSTEM ${__system_params})
  else()
    if(TARGET ${target})
      if(__params)
        target_include_directories(${target} PRIVATE ${__params})
        if(TIGER_DEPENDANT_TARGETS_${target})
          foreach(t ${TIGER_DEPENDANT_TARGETS_${target}})
            target_include_directories(${t} PRIVATE ${__params})
          endforeach()
        endif()
      endif()
      if(__system_params)
        target_include_directories(${target} SYSTEM PRIVATE ${__system_params})
        if(TIGER_DEPENDANT_TARGETS_${target})
          foreach(t ${TIGER_DEPENDANT_TARGETS_${target}})
            target_include_directories(${t} SYSTEM PRIVATE ${__system_params})
          endforeach()
        endif()
      endif()
    else()
      if(__params)
        set(__new_inc ${TI_TARGET_INCLUDE_DIRS_${target}})
        list(APPEND __new_inc ${__params})
        set(TI_TARGET_INCLUDE_DIRS_${target} "${__new_inc}" CACHE INTERNAL "")
      endif()
      if(__system_params)
        set(__new_inc ${TI_TARGET_INCLUDE_SYSTEM_DIRS_${target}})
        list(APPEND __new_inc ${__system_params})
        set(TI_TARGET_INCLUDE_SYSTEM_DIRS_${target} "${__new_inc}" CACHE INTERNAL "")
      endif()
    endif()
  endif()
endfunction()

if(NOT DEFINED CMAKE_ARGC) # Guard CMake standalone invocations

  # Use this option carefully, CMake's install() will install symlinks instead
  # of real files It is fine for development, but should not be used by real
  # installations
  set(__symlink_default OFF) # preprocessing is required for old CMake like
                             # 2.8.12
  if(DEFINED ENV{BUILD_USE_SYMLINKS})
    set(__symlink_default $ENV{BUILD_USE_SYMLINKS})
  endif()
  ti_option(
    BUILD_USE_SYMLINKS
    "Use symlinks instead of files copying during build (and !!INSTALL!!)"
    (${__symlink_default}) IF (UNIX OR DEFINED __symlink_default))

  macro(ti_cmake_byproducts var_name)
    set(${var_name} BYPRODUCTS ${ARGN})
  endmacro()

  set(TIGER_DEPHELPER
      "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/dephelper"
      CACHE INTERNAL "")
  file(MAKE_DIRECTORY ${TIGER_DEPHELPER})

  if(BUILD_USE_SYMLINKS)
    set(__file0 "${CMAKE_CURRENT_LIST_FILE}")
    set(__file1 "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/symlink_test")
    if(NOT IS_SYMLINK "${__file1}")
      execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "${__file0}"
                              "${__file1}" RESULT_VARIABLE SYMLINK_RESULT)
      if(NOT SYMLINK_RESULT EQUAL 0)
        file(REMOVE "${__file1}")
      endif()
      if(NOT IS_SYMLINK "${__file1}")
        set(BUILD_USE_SYMLINKS
            0
            CACHE INTERNAL "")
      endif()
    endif()
    if(NOT BUILD_USE_SYMLINKS)
      message(STATUS "Build symlinks are not available (disabled)")
    endif()
  endif()

  set(TIGER_BUILD_INFO_STR
      ""
      CACHE INTERNAL "")
  function(ti_output_status msg)
    message(STATUS "${msg}")
    string(REPLACE "\\" "\\\\" msg "${msg}")
    string(REPLACE "\"" "\\\"" msg "${msg}")
    string(REGEX REPLACE "^\n+|\n+$" "" msg "${msg}")
    if(msg MATCHES "\n")
      message(
        WARNING
          "String to be inserted to version_string.inc has an unexpected line break: '${msg}'"
      )
      string(REPLACE "\n" "\\n" msg "${msg}")
    endif()
    set(TIGER_BUILD_INFO_STR
        "${TIGER_BUILD_INFO_STR}\"${msg}\\n\"\n"
        CACHE INTERNAL "")
  endfunction()

  macro(ti_finalize_status)
    set(TIGER_BUILD_INFO_FILE "${CMAKE_BINARY_DIR}/version_string.tmp")
    if(EXISTS "${TIGER_BUILD_INFO_FILE}")
      file(READ "${TIGER_BUILD_INFO_FILE}" __content)
    else()
      set(__content "")
    endif()
    if("${__content}" STREQUAL "${TIGER_BUILD_INFO_STR}")
      # message(STATUS "${TIGER_BUILD_INFO_FILE} contains the same content")
    else()
      file(WRITE "${TIGER_BUILD_INFO_FILE}" "${TIGER_BUILD_INFO_STR}")
    endif()
    unset(__content)
    unset(TIGER_BUILD_INFO_STR CACHE)

    if(NOT TIGER_SKIP_STATUS_FINALIZATION)
      if(DEFINED TIGER_MODULE_${TI_INTERNAL_NAME}_core_BINARY_DIR)
        execute_process(
          COMMAND
            ${CMAKE_COMMAND} -E copy_if_different "${TIGER_BUILD_INFO_FILE}"
            "${TIGER_MODULE_${TI_INTERNAL_NAME}_core_BINARY_DIR}/version_string.inc"
          OUTPUT_QUIET)
      endif()
    endif()

    if(UNIX)
      install(
        FILES
          "${${PROJECT_NAME}_SOURCE_DIR}/platforms/scripts/valgrind.supp"
          "${${PROJECT_NAME}_SOURCE_DIR}/platforms/scripts/valgrind_3rdparty.supp"
        DESTINATION "${TIGER_OTHER_INSTALL_PATH}"
        COMPONENT "dev")
    endif()
  endmacro()

  # Status report function. Automatically align right column and selects text
  # based on condition. Usage: status(<text>) status(<heading> <value1>
  # [<value2> ...]) status(<heading> <condition> THEN <text for TRUE> ELSE <text
  # for FALSE> )
  function(status text)
    set(status_cond)
    set(status_then)
    set(status_else)

    set(status_current_name "cond")
    foreach(arg ${ARGN})
      if(arg STREQUAL "THEN")
        set(status_current_name "then")
      elseif(arg STREQUAL "ELSE")
        set(status_current_name "else")
      else()
        list(APPEND status_${status_current_name} ${arg})
      endif()
    endforeach()

    if(DEFINED status_cond)
      set(status_placeholder_length 32)
      string(
        RANDOM
        LENGTH ${status_placeholder_length}
        ALPHABET " " status_placeholder)
      string(LENGTH "${text}" status_text_length)
      if(status_text_length LESS status_placeholder_length)
        string(SUBSTRING "${text}${status_placeholder}" 0
                         ${status_placeholder_length} status_text)
      elseif(DEFINED status_then OR DEFINED status_else)
        ti_output_status("${text}")
        set(status_text "${status_placeholder}")
      else()
        set(status_text "${text}")
      endif()

      if(DEFINED status_then OR DEFINED status_else)
        if(${status_cond})
          string(REPLACE ";" " " status_then "${status_then}")
          string(REGEX REPLACE "^[ \t]+" "" status_then "${status_then}")
          ti_output_status("${status_text} ${status_then}")
        else()
          string(REPLACE ";" " " status_else "${status_else}")
          string(REGEX REPLACE "^[ \t]+" "" status_else "${status_else}")
          ti_output_status("${status_text} ${status_else}")
        endif()
      else()
        string(REPLACE ";" " " status_cond "${status_cond}")
        string(REGEX REPLACE "^[ \t]+" "" status_cond "${status_cond}")
        ti_output_status("${status_text} ${status_cond}")
      endif()
    else()
      ti_output_status("${text}")
    endif()
  endfunction()

endif() # NOT DEFINED CMAKE_ARGC