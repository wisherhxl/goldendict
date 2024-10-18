# Local variables (set for each module):
#
# name       - short name in lower case i.e. core
# the_module - full name in lower case i.e. tiger_core

# Global variables:
#
# TIGER_MODULE_${the_module}_LOCATION
# TIGER_MODULE_${the_module}_BINARY_DIR
# TIGER_MODULE_${the_module}_DESCRIPTION
# TIGER_MODULE_${the_module}_CLASS - PUBLIC|INTERNAL|BINDINGS
# TIGER_MODULE_${the_module}_HEADERS
# TIGER_MODULE_${the_module}_SOURCES
# TIGER_MODULE_${the_module}_DEPS - final flattened set of module dependencies
# TIGER_MODULE_${the_module}_DEPS_TO_LINK - differs from above for world build only
# TIGER_MODULE_${the_module}_DEPS_EXT - non-module dependencies
# TIGER_MODULE_${the_module}_REQ_DEPS
# TIGER_MODULE_${the_module}_OPT_DEPS
# TIGER_MODULE_${the_module}_PRIVATE_REQ_DEPS
# TIGER_MODULE_${the_module}_PRIVATE_OPT_DEPS
# TIGER_MODULE_${the_module}_IS_PART_OF_WORLD
# TIGER_MODULE_${the_module}_CUDA_OBJECTS - compiled CUDA objects list
# TIGER_MODULE_${the_module}_WRAPPERS - list of wrappers supporting this module
# HAVE_${the_module} - for fast check of module availability

# To control the setup of the module you could also set:
# the_description - text to be used as current module description
# the_label - label for current module
# TIGER_MODULE_TYPE - STATIC|SHARED - set to force override global settings for current module
# TIGER_MODULE_IS_PART_OF_WORLD - ON|OFF (default ON) - should the module be added to the tiger_world?
# BUILD_${the_module}_INIT - ON|OFF (default ON) - initial value for BUILD_${the_module}

# The verbose template for Tiger module:
#
#   ti_add_module(modname <dependencies>)
#   ti_glob_module_sources(([EXCLUDE_OPENCL] [EXCLUDE_CUDA] <extra sources&headers>)
#                          or glob them manually and ti_set_module_sources(...)
#   ti_module_include_directories(<extra include directories>)
#   ti_create_module()
#   <add extra link dependencies, compiler options, etc>
#   ti_add_precompiled_headers(${the_module})
#   <add extra installation rules>
#   ti_add_accuracy_tests(<extra dependencies>)
#   ti_add_perf_tests(<extra dependencies>)
#   ti_add_samples(<extra dependencies>)
#
#
# If module have no "extra" then you can define it in one line:
#
#   ti_define_module(modname <dependencies>)

# clean flags for modules enabled on previous cmake run
# this is necessary to correctly handle modules removal
foreach(mod ${TIGER_MODULES_BUILD} ${TIGER_MODULES_DISABLED_USER} ${TIGER_MODULES_DISABLED_AUTO} ${TIGER_MODULES_DISABLED_FORCE})
  if(HAVE_${mod})
    unset(HAVE_${mod} CACHE)
  endif()
  unset(TIGER_MODULE_${mod}_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_DEPS_EXT CACHE)
  unset(TIGER_MODULE_${mod}_REQ_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_OPT_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_PRIVATE_REQ_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_PRIVATE_OPT_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_LINK_DEPS CACHE)
  unset(TIGER_MODULE_${mod}_WRAPPERS CACHE)
endforeach()

# clean modules info which needs to be recalculated
set(TIGER_MODULES_PUBLIC         "" CACHE INTERNAL "List of Tiger modules marked for export")
set(TIGER_MODULES_BUILD          "" CACHE INTERNAL "List of Tiger modules included into the build")
set(TIGER_MODULES_DISABLED_USER  "" CACHE INTERNAL "List of Tiger modules explicitly disabled by user")
set(TIGER_MODULES_DISABLED_AUTO  "" CACHE INTERNAL "List of Tiger modules implicitly disabled due to dependencies")
set(TIGER_MODULES_DISABLED_FORCE "" CACHE INTERNAL "List of Tiger modules which can not be build in current configuration")
unset(TIGER_WORLD_MODULES CACHE)
set(TIGER_SKIP_DLLMAIN_GENERATION 1)
# adds dependencies to Tiger module
# Usage:
#   add_dependencies(tiger_<name> [REQUIRED] [<list of dependencies>] [OPTIONAL <list of modules>] [WRAP <list of wrappers>])
# Notes:
# * <list of dependencies> - can include full names of modules or full paths to shared/static libraries or cmake targets
macro(ti_add_dependencies full_modname)
  ti_debug_message("ti_add_dependencies(" ${full_modname} ${ARGN} ")")
  #we don't clean the dependencies here to allow this macro several times for every module
  foreach(d "REQUIRED" ${ARGN})
    if(d STREQUAL "REQUIRED")
      set(__dephead "")
      set(__depsvar TIGER_MODULE_${full_modname}_REQ_DEPS)
    elseif(d STREQUAL "MODULES")
      set(__dephead "tiger_")
      set(__depsvar TIGER_MODULE_${full_modname}_REQ_DEPS)
    elseif(d STREQUAL "OPTIONAL")
      set(__dephead "")
      set(__depsvar TIGER_MODULE_${full_modname}_OPT_DEPS)
    elseif(d STREQUAL "PRIVATE_REQUIRED")
      set(__dephead "")
      set(__depsvar TIGER_MODULE_${full_modname}_PRIVATE_REQ_DEPS)
    elseif(d STREQUAL "PRIVATE_OPTIONAL")
      set(__dephead "")
      set(__depsvar TIGER_MODULE_${full_modname}_PRIVATE_OPT_DEPS)
    elseif(d STREQUAL "WRAP")
      set(__dephead "")
      set(__depsvar TIGER_MODULE_${full_modname}_WRAPPERS)
    else()
      list(APPEND ${__depsvar} "${__dephead}${d}")
    endif()
  endforeach()
  unset(__dephead)
  unset(__depsvar)

  ti_list_unique(TIGER_MODULE_${full_modname}_REQ_DEPS)
  ti_list_unique(TIGER_MODULE_${full_modname}_OPT_DEPS)
  ti_list_unique(TIGER_MODULE_${full_modname}_PRIVATE_REQ_DEPS)
  ti_list_unique(TIGER_MODULE_${full_modname}_PRIVATE_OPT_DEPS)
  ti_list_unique(TIGER_MODULE_${full_modname}_WRAPPERS)

  set(TIGER_MODULE_${full_modname}_REQ_DEPS ${TIGER_MODULE_${full_modname}_REQ_DEPS}
    CACHE INTERNAL "Required dependencies of ${full_modname} module")
  set(TIGER_MODULE_${full_modname}_OPT_DEPS ${TIGER_MODULE_${full_modname}_OPT_DEPS}
    CACHE INTERNAL "Optional dependencies of ${full_modname} module")
  set(TIGER_MODULE_${full_modname}_PRIVATE_REQ_DEPS ${TIGER_MODULE_${full_modname}_PRIVATE_REQ_DEPS}
    CACHE INTERNAL "Required private dependencies of ${full_modname} module")
  set(TIGER_MODULE_${full_modname}_PRIVATE_OPT_DEPS ${TIGER_MODULE_${full_modname}_PRIVATE_OPT_DEPS}
    CACHE INTERNAL "Optional private dependencies of ${full_modname} module")
  set(TIGER_MODULE_${full_modname}_WRAPPERS ${TIGER_MODULE_${full_modname}_WRAPPERS}
    CACHE INTERNAL "List of wrappers supporting module ${full_modname}")
endmacro()

# declare new Tiger module in current folder
# Usage:
#   ti_add_module(<name> [INTERNAL|BINDINGS] [REQUIRED] [<list of dependencies>] [OPTIONAL <list of optional dependencies>] [WRAP <list of wrappers>])
# Example:
#   ti_add_module(yaom INTERNAL tiger_core tiger_highgui tiger_flann OPTIONAL tiger_cudev)
macro(ti_add_module _name)
  ti_debug_message("ti_add_module(" ${_name} ${ARGN} ")")
  string(TOLOWER "${_name}" name)
  set(the_module tiger_${name})

  # the first pass - collect modules info, the second pass - create targets
  if(TIGER_INITIAL_PASS)
    #guard against redefinition
    if(";${TIGER_MODULES_BUILD};${TIGER_MODULES_DISABLED_USER};" MATCHES ";${the_module};")
      message(FATAL_ERROR "Redefinition of the ${the_module} module.
  at:                    ${CMAKE_CURRENT_SOURCE_DIR}
  previously defined at: ${TIGER_MODULE_${the_module}_LOCATION}
")
    endif()

    if(NOT DEFINED the_description)
      set(the_description "The ${name} Tiger module")
    endif()

    if(NOT DEFINED BUILD_${the_module}_INIT)
      set(BUILD_${the_module}_INIT ON)
    endif()

    # create option to enable/disable this module
    option(BUILD_${the_module} "Include ${the_module} module into the Tiger build" ${BUILD_${the_module}_INIT})

    # remember the module details
    set(TIGER_MODULE_${the_module}_DESCRIPTION "${the_description}" CACHE INTERNAL "Brief description of ${the_module} module")
    set(TIGER_MODULE_${the_module}_LOCATION    "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Location of ${the_module} module sources")

    set(TIGER_MODULE_${the_module}_LINK_DEPS "" CACHE INTERNAL "")

    set(ADD_MODULE_ARGN ${ARGN})
    ti_cmake_hook(PRE_ADD_MODULE)
    ti_cmake_hook(PRE_ADD_MODULE_${the_module})

    # parse list of dependencies
    if(" ${ARGV1}" STREQUAL " INTERNAL" OR " ${ARGV1}" STREQUAL " BINDINGS")
      set(TIGER_MODULE_${the_module}_CLASS "${ARGV1}" CACHE INTERNAL "The category of the module")
      set(__ti_argn__ ${ADD_MODULE_ARGN})
      list(REMOVE_AT __ti_argn__ 0)
      ti_add_dependencies(${the_module} ${__ti_argn__})
      unset(__ti_argn__)
    else()
      set(TIGER_MODULE_${the_module}_CLASS "PUBLIC" CACHE INTERNAL "The category of the module")
      ti_add_dependencies(${the_module} ${ADD_MODULE_ARGN})
      if(BUILD_${the_module})
        set(TIGER_MODULES_PUBLIC ${TIGER_MODULES_PUBLIC} "${the_module}" CACHE INTERNAL "List of Tiger modules marked for export")
      endif()
    endif()

    # add self to the world dependencies
    if((NOT DEFINED TIGER_MODULE_IS_PART_OF_WORLD
        AND NOT TIGER_MODULE_${the_module}_CLASS STREQUAL "BINDINGS"
        AND (NOT DEFINED TIGER_MODULE_${the_module}_IS_PART_OF_WORLD OR TIGER_MODULE_${the_module}_IS_PART_OF_WORLD)
        AND (NOT TIGER_PROCESSING_EXTRA_MODULES OR NOT TIGER_WORLD_EXCLUDE_EXTRA_MODULES)
        AND (NOT BUILD_SHARED_LIBS OR NOT "x${TIGER_MODULE_TYPE}" STREQUAL "xSTATIC"))
        OR TIGER_MODULE_IS_PART_OF_WORLD
        )
      set(TIGER_MODULE_${the_module}_IS_PART_OF_WORLD ON CACHE INTERNAL "")
      ti_add_dependencies(tiger_world OPTIONAL ${the_module})
    else()
      set(TIGER_MODULE_${the_module}_IS_PART_OF_WORLD OFF CACHE INTERNAL "")
    endif()

    if(NOT DEFINED the_label)
      if(TIGER_PROCESSING_EXTRA_MODULES)
        set(the_label "Extra")
      else()
        set(the_label "Main")
      endif()
    endif()
    set(TIGER_MODULE_${the_module}_LABEL "${the_label};${the_module}" CACHE INTERNAL "")

    if(BUILD_${the_module})
      set(TIGER_MODULES_BUILD ${TIGER_MODULES_BUILD} "${the_module}" CACHE INTERNAL "List of Tiger modules included into the build")
    else()
      set(TIGER_MODULES_DISABLED_USER ${TIGER_MODULES_DISABLED_USER} "${the_module}" CACHE INTERNAL "List of Tiger modules explicitly disabled by user")
    endif()

    # stop processing of current file
    ti_cmake_hook(POST_ADD_MODULE)
    ti_cmake_hook(POST_ADD_MODULE_${the_module})
    return()
  else()
    set(TIGER_MODULE_${the_module}_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")
    if(NOT BUILD_${the_module})
      return() # extra protection from redefinition
    endif()
    if(NOT TIGER_MODULE_${the_module}_IS_PART_OF_WORLD OR NOT ${BUILD_tiger_world})
      if (NOT ${the_module} STREQUAL tiger_world)
        project(${the_module})
      endif()
      add_definitions(
        -D_USE_MATH_DEFINES  # M_PI constant in MSVS
        -D__STDC_CONSTANT_MACROS -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS  # to use C libraries from C++ code (ffmpeg)
      )
    endif()
  endif()
endmacro()

# excludes module from current configuration
macro(ti_module_disable_ module)
  set(__modname ${module})
  if(NOT __modname MATCHES "^tiger_")
    set(__modname tiger_${module})
  endif()
  list(APPEND TIGER_MODULES_DISABLED_FORCE "${__modname}")
  set(HAVE_${__modname} OFF CACHE INTERNAL "Module ${__modname} can not be built in current configuration")
  set(TIGER_MODULE_${__modname}_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}" CACHE INTERNAL "Location of ${__modname} module sources")
  set(TIGER_MODULES_DISABLED_FORCE "${TIGER_MODULES_DISABLED_FORCE}" CACHE INTERNAL "List of Tiger modules which can not be build in current configuration")
  if(BUILD_${__modname})
    # touch variable controlling build of the module to suppress "unused variable" CMake warning
  endif()
  unset(__modname)
endmacro()

macro(ti_module_disable module)
  ti_module_disable_(${module})
  return() # leave the current folder
endmacro()

# gather acceptable locations and generate names for them
# if folder contains CMakeLists.txt - it is accepted,
# otherwise all first-level subfolders containing CMakeLists.txt are accepted.
# Usage: _glob_locations(<output paths list> <output names list> <folder> [<folder> ...])
function(_glob_locations out_paths out_names)
  set(PATHS ${ARGN})
  foreach(path ${PATHS})
    #message(STATUS "Inspect: ${path}")
    list(LENGTH paths before)
    get_filename_component(path "${path}" ABSOLUTE)
    # Either module itself
    if(NOT path STREQUAL CMAKE_CURRENT_SOURCE_DIR AND EXISTS "${path}/CMakeLists.txt")
      get_filename_component(name "${path}" NAME)
      list(APPEND paths "${path}")
      list(APPEND names "${name}")
    else()
      # Either flat collection of modules
      file(GLOB subdirs RELATIVE "${path}" "${path}/*")
      foreach(subdir ${subdirs})
        #message(STATUS "Inspect: ${path}/${subdir}")
        if(EXISTS "${path}/${subdir}/CMakeLists.txt")
          list(APPEND paths "${path}/${subdir}")
          list(APPEND names "${subdir}")
        endif()
      endforeach()
    endif()
    list(LENGTH paths after)
    if(before EQUAL after)
      message(SEND_ERROR "No modules has been found: ${path}")
    endif()
  endforeach()
  # Return
  set(${out_paths} ${paths} PARENT_SCOPE)
  set(${out_names} ${names} PARENT_SCOPE)
endfunction()

# Calls 'add_subdirectory' for each location.
# Note: both input lists should have same length.
# Usage: _add_modules_1(<list with paths> <list with names>)
macro(_add_modules_1 paths names)
  ti_debug_message("_add_modules_1(paths=${paths}, names=${names}, ... " ${ARGN} ")")
  list(LENGTH ${paths} __len)
  if(NOT __len EQUAL 0)
    list(LENGTH ${names} __len_verify)
    if(NOT __len EQUAL __len_verify)
      message(FATAL_ERROR "Bad configuration! ${__len} != ${__len_verify}")
    endif()
    math(EXPR __len "${__len} - 1")
    foreach(i RANGE ${__len})
      list(GET ${paths} ${i} __path)
      list(GET ${names} ${i} __name)
      #message(STATUS "First pass: ${__name} => ${__path}")
      include("${__path}/cmake/init.cmake" OPTIONAL)
      add_subdirectory("${__path}" "${CMAKE_CURRENT_BINARY_DIR}/.firstpass/${__name}")
    endforeach()
  endif()
endmacro()

# Calls 'add_subdirectory' for each module name.
# Usage: _add_modules_2([<module> ...])
macro(_add_modules_2)
  ti_debug_message("_add_modules_2(" ${ARGN} ")")
  foreach(m ${ARGN})
    set(the_module "${m}")
    ti_cmake_hook(PRE_MODULES_CREATE_${the_module})
    if(BUILD_tiger_world AND m STREQUAL "tiger_world"
        OR NOT BUILD_tiger_world
        OR NOT TIGER_MODULE_${m}_IS_PART_OF_WORLD)
      if(NOT m MATCHES "^tiger_")
        message(WARNING "Incorrect module name: ${m}")
      endif()
      string(REGEX REPLACE "^tiger_" "" name "${m}")
      #message(STATUS "Second pass: ${name} => ${TIGER_MODULE_${m}_LOCATION}")
      add_subdirectory("${TIGER_MODULE_${m}_LOCATION}" "${CMAKE_CURRENT_BINARY_DIR}/${name}")
    endif()
    ti_cmake_hook(POST_MODULES_CREATE_${the_module})
  endforeach()
  unset(the_module)
endmacro()

# Check if list of input items is unique.
# Usage: _assert_uniqueness(<failure message> <element> [<element> ...])
function(_assert_uniqueness msg)
  ti_get_duplicates(dups ${ARGN})
  if(dups)
    foreach(e ${ARGN})
      list(FIND dups "${e}" idx)
      if(NOT idx EQUAL -1)
        set(prefix " > ")
      else()
        set(prefix "   ")
      endif()
      message("${prefix}${e}")
    endforeach()
    message(FATAL_ERROR "${msg}")
  endif()
endfunction()

# collect modules from specified directories
# NB: must be called only once!
# Usage: ti_glob_modules(<main location> [<extra location> ...])
macro(ti_glob_modules main_root)
  ti_cmake_hook(INIT_MODULES_GLOB)
  if(DEFINED TIGER_INITIAL_PASS)
    message(FATAL_ERROR "Tiger has already loaded its modules. Calling ti_glob_modules second time is not allowed.")
  endif()

  # collect modules
  set(TIGER_INITIAL_PASS ON)
  _glob_locations(__main_paths __main_names ${main_root})
  _glob_locations(__extra_paths __extra_names ${ARGN})
  _assert_uniqueness("Duplicated modules LOCATIONS has been found" ${__main_paths} ${__extra_paths})
  _assert_uniqueness("Duplicated modules NAMES has been found" ${__main_names} ${__extra_names})
  set(TIGER_PROCESSING_EXTRA_MODULES 0)
  ti_cmake_hook(PRE_MODULES_SCAN)
  _add_modules_1(__main_paths __main_names)
  set(TIGER_PROCESSING_EXTRA_MODULES 1)
  ti_cmake_hook(PRE_MODULES_SCAN_EXTRA)
  _add_modules_1(__extra_paths __extra_names)
  ti_clear_vars(__main_names __extra_names __main_paths __extra_paths)
  ti_cmake_hook(POST_MODULES_SCAN)

  # resolve dependencies
  __ti_resolve_dependencies()

  # create modules
  set(TIGER_INITIAL_PASS OFF PARENT_SCOPE)
  set(TIGER_INITIAL_PASS OFF)
  ti_cmake_hook(PRE_MODULES_CREATE)
  _add_modules_2(${TIGER_MODULES_BUILD})
  ti_cmake_hook(POST_MODULES_CREATE)
endmacro()


# disables Tiger module with missing dependencies
function(__ti_module_turn_off the_module)
  list(REMOVE_ITEM TIGER_MODULES_DISABLED_AUTO "${the_module}")
  list(APPEND TIGER_MODULES_DISABLED_AUTO "${the_module}")
  list(REMOVE_ITEM TIGER_MODULES_BUILD "${the_module}")
  list(REMOVE_ITEM TIGER_MODULES_PUBLIC "${the_module}")
  set(HAVE_${the_module} OFF CACHE INTERNAL "Module ${the_module} can not be built in current configuration")

  set(TIGER_MODULES_DISABLED_AUTO "${TIGER_MODULES_DISABLED_AUTO}" CACHE INTERNAL "")
  set(TIGER_MODULES_BUILD "${TIGER_MODULES_BUILD}" CACHE INTERNAL "")
  set(TIGER_MODULES_PUBLIC "${TIGER_MODULES_PUBLIC}" CACHE INTERNAL "")
endfunction()

# sort modules by dependencies
function(__ti_sort_modules_by_deps __lst)
  ti_list_sort(${__lst})
  set(input ${${__lst}})
  set(result "")
  set(result_extra "")
  while(input)
    list(LENGTH input length_before)
    foreach (m ${input})
      # check if module is in the result already
      if (NOT ";${result};" MATCHES ";${m};")
        # scan through module dependencies...
        set(unresolved_deps_found FALSE)
        foreach (d ${TIGER_MODULE_${m}_DEPS})
          # ... which are not already in the result and are enabled
          if ((NOT ";${result};" MATCHES ";${d};") AND HAVE_${d})
            set(unresolved_deps_found TRUE)
            break()
          endif()
        endforeach()
        # check if all dependencies for this module has been resolved
        if (NOT unresolved_deps_found)
          list(APPEND result ${m})
          list(REMOVE_ITEM input ${m})
        endif()
      endif()
    endforeach()
    list(LENGTH input length_after)
    # check for infinite loop or unresolved dependencies
    if (NOT length_after LESS length_before)
      if(NOT BUILD_SHARED_LIBS)
        if (";${input};" MATCHES ";tiger_world;")
          list(REMOVE_ITEM input "tiger_world")
          list(APPEND result_extra "tiger_world")
        else()
          # We can't do here something
          list(APPEND result ${input})
          break()
        endif()
      else()
        message(FATAL_ERROR "FATAL: Unresolved dependencies or loop in dependency graph (${length_after})\n"
          "Processed ${__lst}: ${${__lst}}\n"
          "Good modules: ${result}\n"
          "Bad modules: ${input}"
        )
        list(APPEND result ${input})
        break()
      endif()
    endif()
  endwhile()
  set(${__lst} "${result};${result_extra}" PARENT_SCOPE)
endfunction()

# resolve dependencies
function(__ti_resolve_dependencies)
  foreach(m ${TIGER_MODULES_DISABLED_USER})
    set(HAVE_${m} OFF CACHE INTERNAL "Module ${m} will not be built in current configuration")
  endforeach()
  foreach(m ${TIGER_MODULES_BUILD})
    set(HAVE_${m} ON CACHE INTERNAL "Module ${m} will be built in current configuration")
  endforeach()

  # Whitelist feature
  if(BUILD_LIST)
    # Prepare the list
    string(REGEX REPLACE "[ ,:]+" ";" whitelist "${BUILD_LIST}" )
    if(BUILD_tiger_world)
      list(APPEND whitelist world)
    endif()
    ti_list_add_prefix(whitelist "tiger_")
    ti_list_sort(whitelist)
    ti_list_unique(whitelist)
    message(STATUS "Using whitelist: ${whitelist}")
    # Expand the list
    foreach(depth RANGE 10)
      set(new_whitelist ${whitelist})
      foreach(m ${whitelist})
        list(APPEND new_whitelist ${TIGER_MODULE_${m}_REQ_DEPS})
        list(APPEND new_whitelist ${TIGER_MODULE_${m}_PRIVATE_REQ_DEPS})
      endforeach()
      ti_list_sort(new_whitelist)
      ti_list_unique(new_whitelist)
      if("${whitelist}" STREQUAL "${new_whitelist}")
        break()
      endif()
      set(whitelist "${new_whitelist}")
    endforeach()
    # Disable modules not in whitelist
    foreach(m ${TIGER_MODULES_BUILD})
      list(FIND whitelist ${m} idx)
      if(idx EQUAL -1)
        message(STATUS "Module ${m} disabled by whitelist")
        __ti_module_turn_off(${m})
      endif()
    endforeach()
  endif()

  # add reverse wrapper dependencies (BINDINDS)
  foreach(the_module ${TIGER_MODULES_BUILD})
    foreach (wrapper ${TIGER_MODULE_${the_module}_WRAPPERS})
      if(wrapper STREQUAL "python")  # hack for python (BINDINDS)
        ti_add_dependencies(tiger_python2 OPTIONAL ${the_module})
        ti_add_dependencies(tiger_python3 OPTIONAL ${the_module})
      else()
        ti_add_dependencies(tiger_${wrapper} OPTIONAL ${the_module})
      endif()
      if(DEFINED TIGER_MODULE_tiger_${wrapper}_bindings_generator_CLASS)
        ti_add_dependencies(tiger_${wrapper}_bindings_generator OPTIONAL ${the_module})
      endif()
    endforeach()
  endforeach()

  # disable MODULES with unresolved dependencies
  set(has_changes ON)
  while(has_changes)
    set(has_changes OFF)
    foreach(m ${TIGER_MODULES_BUILD})
      set(__deps ${TIGER_MODULE_${m}_REQ_DEPS} ${TIGER_MODULE_${m}_PRIVATE_REQ_DEPS})
      while(__deps)
        ti_list_pop_front(__deps d)
        string(TOLOWER "${d}" upper_d)
        if(NOT (HAVE_${d} OR HAVE_${upper_d} OR TARGET ${d} OR EXISTS ${d}))
          if(d MATCHES "^tiger_") # TODO Remove this condition in the future and use HAVE_ variables only
            message(STATUS "Module ${m} disabled because ${d} dependency can't be resolved!")
            __ti_module_turn_off(${m})
            set(has_changes ON)
            break()
          else()
            message(STATUS "Assume that non-module dependency is available: ${d} (for module ${m})")
          endif()
        endif()
      endwhile()
    endforeach()
  endwhile()

#  message(STATUS "List of active modules: ${TIGER_MODULES_BUILD}")

  foreach(m ${TIGER_MODULES_BUILD})
    set(deps_${m} ${TIGER_MODULE_${m}_REQ_DEPS})
    foreach(d ${TIGER_MODULE_${m}_OPT_DEPS})
      if(NOT (";${deps_${m}};" MATCHES ";${d};"))
        if(HAVE_${d} OR TARGET ${d})
          list(APPEND deps_${m} ${d})
        endif()
      endif()
    endforeach()
#    message(STATUS "Initial deps of ${m} (w/o private deps): ${deps_${m}}")
  endforeach()

  # propagate dependencies
  set(has_changes ON)
  while(has_changes)
    set(has_changes OFF)
    foreach(m2 ${TIGER_MODULES_BUILD}) # transfer deps of m2 to m
      foreach(m ${TIGER_MODULES_BUILD})
        if((NOT m STREQUAL m2) AND ";${deps_${m}};" MATCHES ";${m2};")
          foreach(d ${deps_${m2}})
            if(NOT (";${deps_${m}};" MATCHES ";${d};"))
#              message(STATUS "  Transfer dependency ${d} from ${m2} to ${m}")
              list(APPEND deps_${m} ${d})
              set(has_changes ON)
            endif()
            if(BUILD_tiger_world
                AND NOT "${m}" STREQUAL "tiger_world"
                AND NOT "${m2}" STREQUAL "tiger_world"
                AND TIGER_MODULE_${m2}_IS_PART_OF_WORLD
                AND NOT TIGER_MODULE_${m}_IS_PART_OF_WORLD)
              if(NOT (";${deps_${m}};" MATCHES ";tiger_world;"))
#                message(STATUS "  Transfer dependency tiger_world alias ${m2} to ${m}")
                list(APPEND deps_${m} tiger_world)
                set(has_changes ON)
              endif()
            endif()
          endforeach()
        endif()
      endforeach()
    endforeach()
  endwhile()

  # process private deps
  foreach(m ${TIGER_MODULES_BUILD})
    foreach(d ${TIGER_MODULE_${m}_PRIVATE_REQ_DEPS})
      if(NOT (";${deps_${m}};" MATCHES ";${d};"))
        list(APPEND deps_${m} ${d})
      endif()
    endforeach()
    foreach(d ${TIGER_MODULE_${m}_PRIVATE_OPT_DEPS})
      if(NOT (";${deps_${m}};" MATCHES ";${d};"))
        if(HAVE_${d} OR TARGET ${d})
          list(APPEND deps_${m} ${d})
        endif()
      endif()
    endforeach()
  endforeach()

  ti_list_sort(TIGER_MODULES_BUILD)

  foreach(m ${TIGER_MODULES_BUILD})
#    message(STATUS "FULL deps of ${m}: ${deps_${m}}")
    set(TIGER_MODULE_${m}_DEPS ${deps_${m}})
    set(TIGER_MODULE_${m}_DEPS_EXT ${deps_${m}})
    ti_list_filterout(TIGER_MODULE_${m}_DEPS_EXT "^tiger_[^ ]+$")
    if(TIGER_MODULE_${m}_DEPS_EXT AND TIGER_MODULE_${m}_DEPS)
      list(REMOVE_ITEM TIGER_MODULE_${m}_DEPS ${TIGER_MODULE_${m}_DEPS_EXT})
    endif()
  endforeach()

  # reorder dependencies
  foreach(m ${TIGER_MODULES_BUILD})
    __ti_sort_modules_by_deps(TIGER_MODULE_${m}_DEPS)

    set(LINK_DEPS ${TIGER_MODULE_${m}_DEPS})

    # process world
    if(BUILD_tiger_world)
      if(TIGER_MODULE_${m}_IS_PART_OF_WORLD)
        list(APPEND TIGER_WORLD_MODULES ${m})
      endif()
      foreach(m2 ${TIGER_MODULES_BUILD})
        if(TIGER_MODULE_${m2}_IS_PART_OF_WORLD)
          if(";${LINK_DEPS};" MATCHES ";${m2};")
            list(REMOVE_ITEM LINK_DEPS ${m2})
            if(NOT (";${LINK_DEPS};" MATCHES ";tiger_world;") AND NOT (${m} STREQUAL tiger_world))
              list(APPEND LINK_DEPS tiger_world)
            endif()
          endif()
          if("${m}" STREQUAL tiger_world)
            list(APPEND TIGER_MODULE_tiger_world_DEPS_EXT ${TIGER_MODULE_${m2}_DEPS_EXT})
          endif()
        endif()
      endforeach()
    endif()

    set(TIGER_MODULE_${m}_DEPS ${TIGER_MODULE_${m}_DEPS} CACHE INTERNAL "Flattened dependencies of ${m} module")
    set(TIGER_MODULE_${m}_DEPS_EXT ${TIGER_MODULE_${m}_DEPS_EXT} CACHE INTERNAL "Extra dependencies of ${m} module")
    set(TIGER_MODULE_${m}_DEPS_TO_LINK ${LINK_DEPS} CACHE INTERNAL "Flattened dependencies of ${m} module (for linker)")

#    message(STATUS "  module deps of ${m}: ${TIGER_MODULE_${m}_DEPS}")
#    message(STATUS "  module link deps of ${m}: ${TIGER_MODULE_${m}_DEPS_TO_LINK}")
#    message(STATUS "  extra deps of ${m}: ${TIGER_MODULE_${m}_DEPS_EXT}")
#    message(STATUS "")
  endforeach()

  __ti_sort_modules_by_deps(TIGER_MODULES_BUILD)

  set(TIGER_MODULES_PUBLIC        ${TIGER_MODULES_PUBLIC}        CACHE INTERNAL "List of Tiger modules marked for export")
  set(TIGER_MODULES_BUILD         ${TIGER_MODULES_BUILD}         CACHE INTERNAL "List of Tiger modules included into the build")
  set(TIGER_MODULES_DISABLED_AUTO ${TIGER_MODULES_DISABLED_AUTO} CACHE INTERNAL "List of Tiger modules implicitly disabled due to dependencies")
  set(TIGER_WORLD_MODULES         ${TIGER_WORLD_MODULES}         CACHE INTERNAL "List of Tiger modules included into the world")
endfunction()


# setup include paths for the list of passed modules
macro(ti_include_modules)
  foreach(d ${ARGN})
    if(d MATCHES "^tiger_" AND HAVE_${d})
      if (EXISTS "${TIGER_MODULE_${d}_LOCATION}/include")
        ti_include_directories("${TIGER_MODULE_${d}_LOCATION}/include")
      endif()
    elseif(EXISTS "${d}")
      ti_include_directories("${d}")
    endif()
  endforeach()
endmacro()

# same as previous but with dependencies
macro(ti_include_modules_recurse)
  ti_include_modules(${ARGN})
  foreach(d ${ARGN})
    if(d MATCHES "^tiger_" AND HAVE_${d} AND DEFINED TIGER_MODULE_${d}_DEPS)
      foreach (sub ${TIGER_MODULE_${d}_DEPS})
        ti_include_modules(${sub})
      endforeach()
    endif()
  endforeach()
endmacro()

# setup include paths for the list of passed modules
macro(ti_target_include_modules target)
  foreach(d ${ARGN})
    if(d MATCHES "^tiger_")
      if(HAVE_${d} AND EXISTS "${TIGER_MODULE_${d}_LOCATION}/include")
        ti_target_include_directories(${target} "${TIGER_MODULE_${d}_LOCATION}/include")
      endif()
    elseif(EXISTS "${d}")
      ti_target_include_directories(${target} "${d}")
    else()
      message(WARNING "Unexpected include: ${d} (module=${the_module})")
    endif()
  endforeach()
endmacro()

# setup include paths for the list of passed modules and recursively add dependent modules
macro(ti_target_include_modules_recurse target)
  foreach(d ${ARGN})
    if(d MATCHES "^tiger_" AND HAVE_${d})
      if (EXISTS "${TIGER_MODULE_${d}_LOCATION}/include")
        ti_target_include_directories(${target} "${TIGER_MODULE_${d}_LOCATION}/include")
      endif()
      if(TIGER_MODULE_${d}_DEPS)
        ti_target_include_modules(${target} ${TIGER_MODULE_${d}_DEPS})
      endif()
    elseif(EXISTS "${d}")
      ti_target_include_directories(${target} "${d}")
    endif()
  endforeach()
endmacro()

# setup include path for Tiger headers for specified module
# ti_module_include_directories(<extra include directories/extra include modules>)
macro(ti_module_include_directories)
  if(ENABLE_PRECOMPILED_HEADERS OR TIGER_INCLUDE_DIR_APPEND_MODULE_SRC)
    ti_target_include_directories(${the_module} "${TIGER_MODULE_${the_module}_LOCATION}/src")
  endif()
  ti_target_include_directories(${the_module}
      "${TIGER_MODULE_${the_module}_LOCATION}/include"
      "${CMAKE_CURRENT_BINARY_DIR}" # for precompiled headers
      )
  ti_target_include_modules(${the_module} ${TIGER_MODULE_${the_module}_DEPS} ${ARGN})
endmacro()


# sets header and source files for the current module
# NB: all files specified as headers will be installed
# Usage:
# ti_set_module_sources([HEADERS] <list of files> [SOURCES] <list of files>)
macro(ti_set_module_sources)
  ti_debug_message("ti_set_module_sources(" ${ARGN} ")")

  set(TIGER_MODULE_${the_module}_HEADERS "")
  set(TIGER_MODULE_${the_module}_SOURCES "")

  foreach(f "HEADERS" ${ARGN})
    if(f STREQUAL "HEADERS" OR f STREQUAL "SOURCES")
      set(__filesvar "TIGER_MODULE_${the_module}_${f}")
    else()
      list(APPEND ${__filesvar} "${f}")
    endif()
  endforeach()

  # the hacky way to embed any files into the Tiger without modification of its build system
  if(COMMAND ti_get_module_external_sources)
    ti_get_module_external_sources()
  endif()

  if(TIGER_MODULE_${the_module}_SOURCES_DISPATCHED)
    list(APPEND TIGER_MODULE_${the_module}_SOURCES ${TIGER_MODULE_${the_module}_SOURCES_DISPATCHED})
  endif()

  # TODO Update hooks above
  ti_cmake_hook(INIT_MODULE_SOURCES)
  ti_cmake_hook(INIT_MODULE_SOURCES_${the_module})

  # use full paths for module to be independent from the module location
  ti_convert_to_full_paths(TIGER_MODULE_${the_module}_HEADERS)

  set(TIGER_MODULE_${the_module}_HEADERS ${TIGER_MODULE_${the_module}_HEADERS} CACHE INTERNAL "List of header files for ${the_module}")
  set(TIGER_MODULE_${the_module}_SOURCES ${TIGER_MODULE_${the_module}_SOURCES} CACHE INTERNAL "List of source files for ${the_module}")
endmacro()

# finds and sets headers and sources for the standard Tiger module
# Usage:
# ti_glob_module_sources([EXCLUDE_CUDA] [EXCLUDE_OPENCL] <extra sources&headers in the same format as used in ti_set_module_sources>)
macro(ti_glob_module_sources)
  ti_debug_message("ti_glob_module_sources(" ${ARGN} ")")
  set(_argn ${ARGN})
  list(FIND _argn "EXCLUDE_CUDA" exclude_cuda)
  if(NOT exclude_cuda EQUAL -1)
    list(REMOVE_AT _argn ${exclude_cuda})
  endif()
  list(FIND _argn "EXCLUDE_OPENCL" exclude_opencl)
  if(NOT exclude_opencl EQUAL -1)
    list(REMOVE_AT _argn ${exclude_opencl})
  endif()

  file(GLOB_RECURSE lib_srcs
       "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
  )
  file(GLOB_RECURSE lib_int_hdrs
       "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/src/*.h"
  )
  file(GLOB lib_hdrs
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/*.h"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/hal/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/hal/*.h"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/utils/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/utils/*.h"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/legacy/*.h"
  )
  file(GLOB lib_hdrs_detail
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/detail/*.hpp"
       "${CMAKE_CURRENT_LIST_DIR}/include/tiger/${name}/detail/*.h"
  )

  ti_source_group("Src" DIRBASE "${CMAKE_CURRENT_LIST_DIR}/src" FILES ${lib_srcs} ${lib_int_hdrs})
  ti_source_group("Include" DIRBASE "${CMAKE_CURRENT_LIST_DIR}/include" FILES ${lib_hdrs} ${lib_hdrs_detail})

  set(lib_cuda_srcs "")
  set(lib_cuda_hdrs "")

  ti_set_module_sources(${_argn} HEADERS ${lib_hdrs} ${lib_hdrs_detail}
                         SOURCES ${lib_srcs} ${lib_int_hdrs} ${lib_cuda_srcs} ${lib_cuda_hdrs})
endmacro()

# creates Tiger module in current folder
# creates new target, configures standard dependencies, compilers flags, install rules
# Usage:
#   ti_create_module(<extra link dependencies>)
#   ti_create_module()
macro(ti_create_module)
  ti_debug_message("${the_module}: ti_create_module(" ${ARGN} ")")
  if(TIGER_MODULE_${the_module}_CLASS STREQUAL "BINDINGS")
    message(FATAL_ERROR "Bindings module can't call ti_create_module()")
  endif()
  if(NOT " ${ARGN}" STREQUAL " ")
    set(TIGER_MODULE_${the_module}_LINK_DEPS "${TIGER_MODULE_${the_module}_LINK_DEPS};${ARGN}" CACHE INTERNAL "")
  endif()
  if(BUILD_tiger_world AND TIGER_MODULE_${the_module}_IS_PART_OF_WORLD)
    # nothing
    set(the_module_target tiger_world)
  else()
    _ti_create_module(${ARGN})
    set(the_module_target ${the_module})
  endif()

  if(WINRT AND BUILD_TESTS)
    # removing APPCONTAINER from modules to run from console
    # in case of usual starting of WinRT test apps output is missing
    # so starting of console version w/o APPCONTAINER is required to get test results
    # also this allows to use tiger_extra test data for these tests
    if(NOT "${the_module}" STREQUAL "tiger_ts" AND NOT "${the_module}" STREQUAL "tiger_hal")
      add_custom_command(TARGET ${the_module}
                         POST_BUILD
                         COMMAND link.exe /edit /APPCONTAINER:NO $(TargetPath))
    endif()

    if("${the_module}" STREQUAL "tiger_ts")
      # copy required dll files; WinRT apps need these dlls that are usually substituted by Visual Studio
      # however they are not on path and need to be placed with executables to run from console w/o APPCONTAINER
      add_custom_command(TARGET ${the_module}
        POST_BUILD
        COMMAND copy /y "\"$(VCInstallDir)redist\\$(PlatformTarget)\\Microsoft.VC$(PlatformToolsetVersion).CRT\\msvcp$(PlatformToolsetVersion).dll\"" "\"${CMAKE_BINARY_DIR}\\bin\\$(Configuration)\\msvcp$(PlatformToolsetVersion)_app.dll\""
        COMMAND copy /y "\"$(VCInstallDir)redist\\$(PlatformTarget)\\Microsoft.VC$(PlatformToolsetVersion).CRT\\msvcr$(PlatformToolsetVersion).dll\"" "\"${CMAKE_BINARY_DIR}\\bin\\$(Configuration)\\msvcr$(PlatformToolsetVersion)_app.dll\""
        COMMAND copy /y "\"$(VCInstallDir)redist\\$(PlatformTarget)\\Microsoft.VC$(PlatformToolsetVersion).CRT\\vccorlib$(PlatformToolsetVersion).dll\"" "\"${CMAKE_BINARY_DIR}\\bin\\$(Configuration)\\vccorlib$(PlatformToolsetVersion)_app.dll\"")
    endif()
  endif()
endmacro()

macro(_ti_create_module)

  ti_compiler_optimization_process_sources(TIGER_MODULE_${the_module}_SOURCES TIGER_MODULE_${the_module}_DEPS_EXT ${the_module})
  set(TIGER_MODULE_${the_module}_HEADERS ${TIGER_MODULE_${the_module}_HEADERS} CACHE INTERNAL "List of header files for ${the_module}")
  set(TIGER_MODULE_${the_module}_SOURCES ${TIGER_MODULE_${the_module}_SOURCES} CACHE INTERNAL "List of source files for ${the_module}")

  # The condition we ought to be testing here is whether ti_add_precompiled_headers will
  # be called at some point in the future. We can't look into the future, though,
  # so this will have to do.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/precomp.hpp" AND NOT ${the_module} STREQUAL tiger_world)
    get_native_precompiled_header(${the_module} precomp.hpp)
  endif()

  if(WIN32
      AND (BUILD_SHARED_LIBS AND NOT "x${TIGER_MODULE_TYPE}" STREQUAL "xSTATIC")
      AND NOT TIGER_VS_VERSIONINFO_SKIP)
    if(DEFINED TIGER_VS_VERSIONINFO_FILE)
      set(_VS_VERSION_FILE "${TIGER_VS_VERSIONINFO_FILE}")
    elseif(DEFINED TIGER_VS_VERSIONINFO_${the_module}_FILE)
      set(_VS_VERSION_FILE "${TIGER_VS_VERSIONINFO_${the_module}_FILE}")
    elseif(NOT TIGER_VS_VERSIONINFO_SKIP_GENERATION)
      set(_VS_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/vs_version.rc")
      ti_generate_vs_version_file("${_VS_VERSION_FILE}"
        NAME "${the_module}"
        FILEDESCRIPTION "Tiger module: ${TIGER_MODULE_${the_module}_DESCRIPTION}"
        INTERNALNAME "${the_module}${TIGER_DLLVERSION}"
        ORIGINALFILENAME "${the_module}${TIGER_DLLVERSION}.dll"
      )
    endif()
    if(_VS_VERSION_FILE)
      if(NOT EXISTS "${_VS_VERSION_FILE}")
        message(STATUS "${the_module}: Required .rc file is missing: ${_VS_VERSION_FILE}")
      endif()
      source_group("Src" FILES "${_VS_VERSION_FILE}")
    endif()
  endif()
  if(WIN32 AND NOT (
          "${the_module}" STREQUAL "tiger_core" OR
          "${the_module}" STREQUAL "tiger_world" OR
          "${the_module}" STREQUAL "tiger_cudev"
      )
      AND (BUILD_SHARED_LIBS AND NOT "x${TIGER_MODULE_TYPE}" STREQUAL "xSTATIC")
      AND NOT TIGER_SKIP_DLLMAIN_GENERATION
  )
      set(_DLLMAIN_FILE "${CMAKE_CURRENT_BINARY_DIR}/${the_module}_main.cpp")
      configure_file("${Tiger_SOURCE_DIR}/cmake/templates/dllmain.cpp.in" "${_DLLMAIN_FILE}" @ONLY)
  endif()

  source_group("Include" FILES "${TIGER_CONFIG_FILE_INCLUDE_DIR}/ticonfig.h" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/tiger/tiger_modules.hpp")
  source_group("Src" FILES "${${the_module}_pch}")
  ti_cmake_hook(PRE_CREATE_MODULE_LIBRARY)
  ti_cmake_hook(PRE_CREATE_MODULE_LIBRARY_${the_module})
  ti_add_library(${the_module} ${TIGER_MODULE_TYPE} ${TIGER_MODULE_${the_module}_HEADERS} ${TIGER_MODULE_${the_module}_SOURCES}
    "${TIGER_CONFIG_FILE_INCLUDE_DIR}/ticonfig.h" "${TIGER_CONFIG_FILE_INCLUDE_DIR}/tiger/tiger_modules.hpp"
    ${${the_module}_pch}
    ${_VS_VERSION_FILE}
    ${_DLLMAIN_FILE}
  )
  set_target_properties(${the_module} PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};Module")
  set_source_files_properties(${TIGER_MODULE_${the_module}_HEADERS} ${TIGER_MODULE_${the_module}_SOURCES} ${${the_module}_pch}
    PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};Module")

  ti_target_link_libraries(${the_module} PUBLIC    ${TIGER_MODULE_${the_module}_DEPS_TO_LINK}
                                          INTERFACE ${TIGER_MODULE_${the_module}_DEPS_TO_LINK}
  )
  ti_target_link_libraries(${the_module} PUBLIC    ${TIGER_MODULE_${the_module}_DEPS_EXT}
                                          INTERFACE ${TIGER_MODULE_${the_module}_DEPS_EXT}
  )
  ti_target_link_libraries(${the_module} PRIVATE ${TIGER_LINKER_LIBS} ${TIGER_HAL_LINKER_LIBS} ${IPP_LIBS} ${ARGN})
  if (HAVE_CUDA)
    ti_target_link_libraries(${the_module} PRIVATE ${CUDA_LIBRARIES} ${CUDA_npp_LIBRARY})
  endif()

  if(TIGER_MODULE_${the_module}_COMPILE_DEFINITIONS)
    target_compile_definitions(${the_module} ${TIGER_MODULE_${the_module}_COMPILE_DEFINITIONS})
    unset(TIGER_MODULE_${the_module}_COMPILE_DEFINITIONS CACHE)
  endif()

  add_dependencies(tiger_modules ${the_module})

  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(${the_module} PROPERTIES FOLDER "modules")
  endif()

  set_target_properties(${the_module} PROPERTIES
    OUTPUT_NAME "${the_module}${TIGER_DLLVERSION}"
    DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
    COMPILE_PDB_NAME "${the_module}${TIGER_DLLVERSION}"
    COMPILE_PDB_NAME_DEBUG "${the_module}${TIGER_DLLVERSION}${TIGER_DEBUG_POSTFIX}"
    ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
    COMPILE_PDB_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
    RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
    DEFINE_SYMBOL TIAPI_EXPORTS
  )

  if(BUILD_FAT_JAVA_LIB)  # force exports from static modules too
    if(BUILD_SHARED_LIBS)
      message(FATAL_ERROR "Assertion failed: BUILD_SHARED_LIBS=OFF must be off if BUILD_FAT_JAVA_LIB=ON")
    endif()
    target_compile_definitions(${the_module} PRIVATE TIAPI_EXPORTS)
  endif()

  # For dynamic link numbering conventions
  if(NOT ANDROID)
    # Android SDK build scripts can include only .so files into final .apk
    # As result we should not set version properties for Android
    set_target_properties(${the_module} PROPERTIES
      VERSION ${TIGER_LIBVERSION}
      SOVERSION ${TIGER_SOVERSION}
    )
  endif()

  if (ENABLE_GNU_STL_DEBUG)
    target_compile_definitions(${the_module} PUBLIC _GLIBCXX_DEBUG)
  endif()

  if(MSVC)
    if(CMAKE_CROSSCOMPILING)
      set_target_properties(${the_module} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:secchk")
    endif()
    set_target_properties(${the_module} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:libc /DEBUG")
  endif()

  get_target_property(_target_type ${the_module} TYPE)
  if(TIGER_MODULE_${the_module}_CLASS STREQUAL "PUBLIC" AND
      ("${_target_type}" STREQUAL "SHARED_LIBRARY" OR (NOT BUILD_SHARED_LIBS OR NOT INSTALL_CREATE_DISTRIB)))
    ti_install_target(${the_module} EXPORT TigerModules OPTIONAL
      RUNTIME DESTINATION ${TIGER_BIN_INSTALL_PATH} COMPONENT libs
      LIBRARY DESTINATION ${TIGER_LIB_INSTALL_PATH} COMPONENT libs NAMELINK_SKIP
      ARCHIVE DESTINATION ${TIGER_LIB_ARCHIVE_INSTALL_PATH} COMPONENT dev
      )
  endif()
  if("${_target_type}" STREQUAL "SHARED_LIBRARY")
    install(TARGETS ${the_module}
      LIBRARY DESTINATION ${TIGER_LIB_INSTALL_PATH} COMPONENT dev NAMELINK_ONLY)
  endif()

  # only "public" headers need to be installed
  ti_cmake_hook(PRE_INSTALL_MODULE_HEADERS)
  ti_cmake_hook(PRE_INSTALL_MODULE_HEADERS_${the_module})
  if(TIGER_MODULE_${the_module}_HEADERS AND ";${TIGER_MODULES_PUBLIC};" MATCHES ";${the_module};")
    foreach(hdr ${TIGER_MODULE_${the_module}_HEADERS})
      string(REGEX REPLACE "^.*tiger/" "tiger/" hdr2 "${hdr}")
      if(NOT hdr2 MATCHES "private" AND hdr2 MATCHES "^(tiger/?.*)/[^/]+.h(..)?$" )
        install(FILES ${hdr} OPTIONAL DESTINATION "${TIGER_INCLUDE_INSTALL_PATH}/${CMAKE_MATCH_1}" COMPONENT dev)
      else()
        #message("Header file will be NOT installed: ${hdr}")
      endif()
    endforeach()
  endif()

  _ti_add_precompiled_headers(${the_module})

  ti_cmake_hook(POST_CREATE_MODULE_LIBRARY)
  ti_cmake_hook(POST_CREATE_MODULE_LIBRARY_${the_module})
endmacro()

# tiger precompiled headers macro (can add pch to modules and tests)
# this macro must be called after any "add_definitions" commands, otherwise precompiled headers will not work
# Usage:
# ti_add_precompiled_headers(${the_module})
macro(_ti_add_precompiled_headers the_target)
  ti_debug_message("ti_add_precompiled_headers(" ${the_target} ${ARGN} ")")

  if("${the_target}" MATCHES "^tiger_test_.*$")
    SET(pch_path "test/test_")
  elseif("${the_target}" MATCHES "^tiger_perf_.*$")
    SET(pch_path "perf/perf_")
  else()
    SET(pch_path "src/")
  endif()
  ti_add_precompiled_header_to_target(${the_target} "${CMAKE_CURRENT_SOURCE_DIR}/${pch_path}precomp.hpp")
  unset(pch_path)
endmacro()

# short command for adding simple Tiger module
# see ti_add_module for argument details
# Usage:
# ti_define_module(module_name  [INTERNAL] [EXCLUDE_CUDA] [REQUIRED] [<list of dependencies>] [OPTIONAL <list of optional dependencies>] [WRAP <list of wrappers>])
macro(ti_define_module module_name)
  ti_debug_message("ti_define_module(" ${module_name} ${ARGN} ")")
  set(_argn ${ARGN})
  set(exclude_cuda "")
  foreach(arg ${_argn})
    if("${arg}" STREQUAL "EXCLUDE_CUDA")
      set(exclude_cuda "${arg}")
      list(REMOVE_ITEM _argn ${arg})
    endif()
  endforeach()

  ti_add_module(${module_name} ${_argn})
  ti_glob_module_sources(${exclude_cuda})
  ti_module_include_directories()
  ti_create_module()

  ti_add_tests(${module_name})
  # ti_add_accuracy_tests()
  # ti_add_perf_tests()
  # ti_add_samples()
endmacro()

# ensures that all passed modules are available
# sets TI_DEPENDENCIES_FOUND variable to TRUE/FALSE
macro(ti_check_dependencies)
  set(TI_DEPENDENCIES_FOUND TRUE)
  foreach(d ${ARGN})
    if(d MATCHES "^tiger_[^ ]+$")
      if(NOT HAVE_${d})
        message(FATAL_ERROR "Cannot find Tiger module: ${d}")
        set(TI_DEPENDENCIES_FOUND FALSE)
        break()
      endif()
    else()
      message(FATAL_ERROR "Tiger module should be called with prefix tiger_, e.g. tiger_base for module base.")
    endif()
  endforeach()
endmacro()


################################################################################
# Tiger tests
################################################################################

if(DEFINED TIGER_BUILD_TEST_MODULES_LIST)
  string(REPLACE "," ";" TIGER_BUILD_TEST_MODULES_LIST "${TIGER_BUILD_TEST_MODULES_LIST}")  # support comma-separated list (,) too
endif()
if(DEFINED TIGER_BUILD_PERF_TEST_MODULES_LIST)
  string(REPLACE "," ";" TIGER_BUILD_PERF_TEST_MODULES_LIST "${TIGER_BUILD_PERF_TEST_MODULES_LIST}")  # support comma-separated list (,) too
endif()

# auxiliary macro to parse arguments of ti_add_accuracy_tests and ti_add_perf_tests commands
macro(__ti_parse_test_sources tests_type)
  set(TIGER_${tests_type}_${the_module}_SOURCES "")
  set(TIGER_${tests_type}_${the_module}_DEPS "")
  set(__file_group_name "")
  set(__file_group_sources "")
  foreach(arg "DEPENDS_ON" ${ARGN} "FILES")
    if(arg STREQUAL "FILES")
      set(__currentvar "__file_group_sources")
      if(__file_group_name AND __file_group_sources)
        source_group("${__file_group_name}" FILES ${__file_group_sources})
        list(APPEND TIGER_${tests_type}_${the_module}_SOURCES ${__file_group_sources})
      endif()
      set(__file_group_name "")
      set(__file_group_sources "")
    elseif(arg STREQUAL "DEPENDS_ON")
      set(__currentvar "TIGER_${tests_type}_${the_module}_DEPS")
    elseif(" ${__currentvar}" STREQUAL " __file_group_sources" AND NOT __file_group_name) # spaces to avoid CMP0054
      set(__file_group_name "${arg}")
    else()
      list(APPEND ${__currentvar} "${arg}")
    endif()
  endforeach()
  unset(__file_group_name)
  unset(__file_group_sources)
  unset(__currentvar)
endmacro()

ti_check_environment_variables(TIGER_TEST_EXTRA_CXX_FLAGS_Release)

# this is a command for adding Tiger performance tests to the module
# ti_add_perf_tests(<extra_dependencies>)
function(ti_add_perf_tests)
  ti_debug_message("ti_add_perf_tests(" ${ARGN} ")")

  if(WINRT)
    set(TIGER_DEBUG_POSTFIX "")
  endif()

  set(perf_path "${CMAKE_CURRENT_LIST_DIR}/perf")
  if(BUILD_PERF_TESTS AND EXISTS "${perf_path}"
      AND (NOT DEFINED TIGER_BUILD_PERF_TEST_MODULES_LIST
          OR TIGER_BUILD_PERF_TEST_MODULES_LIST STREQUAL "all"
          OR ";${TIGER_BUILD_PERF_TEST_MODULES_LIST};" MATCHES ";${name};"
      )
  )
    __ti_parse_test_sources(PERF ${ARGN})

    # tiger_imgcodecs is required for imread/imwrite
    set(perf_deps tiger_ts ${the_module} tiger_imgcodecs ${TIGER_MODULE_${the_module}_DEPS} ${TIGER_MODULE_tiger_ts_DEPS})
    ti_check_dependencies(${perf_deps})

    if(TI_DEPENDENCIES_FOUND)
      set(the_target "tiger_perf_${name}")
      # project(${the_target})

      if(NOT TIGER_PERF_${the_module}_SOURCES)
        file(GLOB_RECURSE perf_srcs "${perf_path}/*.cpp")
        file(GLOB_RECURSE perf_hdrs "${perf_path}/*.hpp" "${perf_path}/*.h")
        ti_source_group("Src" DIRBASE "${perf_path}" FILES ${perf_srcs})
        ti_source_group("Include" DIRBASE "${perf_path}" FILES ${perf_hdrs})
        set(TIGER_PERF_${the_module}_SOURCES ${perf_srcs} ${perf_hdrs})
      endif()

      ti_compiler_optimization_process_sources(TIGER_PERF_${the_module}_SOURCES TIGER_PERF_${the_module}_DEPS ${the_target})

      if(NOT BUILD_tiger_world)
        get_native_precompiled_header(${the_target} perf_precomp.hpp)
      endif()

      source_group("Src" FILES "${${the_target}_pch}")
      ti_add_executable(${the_target} ${TIGER_PERF_${the_module}_SOURCES} ${${the_target}_pch})
      ti_target_include_modules(${the_target} ${perf_deps})
      ti_target_link_libraries(${the_target} PRIVATE ${perf_deps} ${TIGER_MODULE_${the_module}_DEPS} ${TIGER_LINKER_LIBS} ${TIGER_PERF_${the_module}_DEPS})
      add_dependencies(tiger_perf_tests ${the_target})

      if(TARGET tiger_videoio_plugins)
        add_dependencies(${the_target} tiger_videoio_plugins)
      endif()

      if(HAVE_HPX)
        message("Linking HPX to Perf test of module ${name}")
        ti_target_link_libraries(${the_target} LINK_PRIVATE "${HPX_LIBRARIES}")
      endif()

      set_target_properties(${the_target} PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};PerfTest")
      set_source_files_properties(${TIGER_PERF_${the_module}_SOURCES} ${${the_target}_pch}
        PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};PerfTest")

      # Additional target properties
      set_target_properties(${the_target} PROPERTIES
        DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
        RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
      )
      if(ENABLE_SOLUTION_FOLDERS)
        set_target_properties(${the_target} PROPERTIES FOLDER "tests performance")
      endif()

      if(WINRT)
        # removing APPCONTAINER from tests to run from console
        # look for detailed description inside of ti_create_module macro above
        add_custom_command(TARGET "tiger_perf_${name}"
                           POST_BUILD
                           COMMAND link.exe /edit /APPCONTAINER:NO $(TargetPath))
      endif()

      if(NOT BUILD_tiger_world)
        _ti_add_precompiled_headers(${the_target})
      endif()

      ti_add_test_from_target("${the_target}" "Performance" "${the_target}")
      ti_add_test_from_target("tiger_sanity_${name}" "Sanity" "${the_target}"
                               "--perf_min_samples=1"
                               "--perf_force_samples=1"
                               "--perf_verify_sanity")
    else(TI_DEPENDENCIES_FOUND)
      # TODO: warn about unsatisfied dependencies
    endif(TI_DEPENDENCIES_FOUND)
    if(INSTALL_TESTS)
      install(TARGETS ${the_target} RUNTIME DESTINATION ${TIGER_TEST_INSTALL_PATH} COMPONENT tests)
    endif()
  endif()
endfunction()

# this is a command for adding Tiger tests to the module
# ti_add_tests(module_name)
macro(ti_add_tests module_name)
  file(GLOB_RECURSE src_cpp
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/tests/
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
  )
  foreach(test_src ${src_cpp})
    get_filename_component(test_name ${test_src} NAME_WE)
    get_filename_component(test_path ${test_src} PATH)
    
    ti_add_executable(${test_name} ${CMAKE_CURRENT_SOURCE_DIR}/tests/${test_src})
    ti_target_include_modules_recurse(${test_name} tiger_${module_name})
    ti_target_link_libraries(${test_name} tiger_${module_name})
    target_include_directories(${test_name}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/test/
    )

    add_test(NAME ${test_name} COMMAND ${test_name})
    set_property(TARGET ${test_name} 
      PROPERTY
        FOLDER "tests/${module_name}/${test_path}"
    )
  endforeach()
  
endmacro()

# this is a command for adding Tiger accuracy/regression tests to the module
# ti_add_accuracy_tests(<list of extra dependencies>)
function(ti_add_accuracy_tests)
  ti_debug_message("ti_add_accuracy_tests(" ${ARGN} ")")

  set(test_path "${CMAKE_CURRENT_LIST_DIR}/test")
  if(BUILD_TESTS AND EXISTS "${test_path}"
      AND (NOT DEFINED TIGER_BUILD_TEST_MODULES_LIST
          OR TIGER_BUILD_TEST_MODULES_LIST STREQUAL "all"
          OR ";${TIGER_BUILD_TEST_MODULES_LIST};" MATCHES ";${name};"
      )
  )
    __ti_parse_test_sources(TEST ${ARGN})

    # tiger_imgcodecs is required for imread/imwrite
    set(test_deps tiger_ts ${the_module} tiger_imgcodecs tiger_videoio ${TIGER_MODULE_${the_module}_DEPS} ${TIGER_MODULE_tiger_ts_DEPS})
    ti_check_dependencies(${test_deps})
    if(TI_DEPENDENCIES_FOUND)
      set(the_target "tiger_test_${name}")
      # project(${the_target})

      if(NOT TIGER_TEST_${the_module}_SOURCES)
        file(GLOB_RECURSE test_srcs "${test_path}/*.cpp")
        file(GLOB_RECURSE test_hdrs "${test_path}/*.hpp" "${test_path}/*.h")
        ti_source_group("Src" DIRBASE "${test_path}" FILES ${test_srcs})
        ti_source_group("Include" DIRBASE "${test_path}" FILES ${test_hdrs})
        set(TIGER_TEST_${the_module}_SOURCES ${test_srcs} ${test_hdrs})
      endif()

      if(TIGER_MODULE_${the_module}_TEST_SOURCES_DISPATCHED)
        list(APPEND TIGER_TEST_${the_module}_SOURCES ${TIGER_MODULE_${the_module}_TEST_SOURCES_DISPATCHED})
      endif()
      ti_compiler_optimization_process_sources(TIGER_TEST_${the_module}_SOURCES TIGER_TEST_${the_module}_DEPS ${the_target})

      if(NOT BUILD_tiger_world)
        get_native_precompiled_header(${the_target} test_precomp.hpp)
      endif()

      source_group("Src" FILES "${${the_target}_pch}")
      ti_add_executable(${the_target} ${TIGER_TEST_${the_module}_SOURCES} ${${the_target}_pch})
      ti_target_include_modules(${the_target} ${test_deps})
      if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/test")
        ti_target_include_directories(${the_target} "${CMAKE_CURRENT_BINARY_DIR}/test")
      endif()
      ti_target_link_libraries(${the_target} PRIVATE ${test_deps} ${TIGER_MODULE_${the_module}_DEPS} ${TIGER_LINKER_LIBS} ${TIGER_TEST_${the_module}_DEPS})
      add_dependencies(tiger_tests ${the_target})

      if(TARGET tiger_videoio_plugins)
        add_dependencies(${the_target} tiger_videoio_plugins)
      endif()

      if(HAVE_HPX)
        message("Linking HPX to Perf test of module ${name}")
        ti_target_link_libraries(${the_target} LINK_PRIVATE "${HPX_LIBRARIES}")
      endif()

      set_target_properties(${the_target} PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};AccuracyTest")
      set_source_files_properties(${TIGER_TEST_${the_module}_SOURCES} ${${the_target}_pch}
        PROPERTIES LABELS "${TIGER_MODULE_${the_module}_LABEL};AccuracyTest")

      # Additional target properties
      set_target_properties(${the_target} PROPERTIES
        DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
        RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
      )

      ti_append_target_property(${the_target} COMPILE_DEFINITIONS "__TIGER_TESTS=1")

      if(ENABLE_SOLUTION_FOLDERS)
        set_target_properties(${the_target} PROPERTIES FOLDER "tests accuracy")
      endif()

      if(TIGER_TEST_BIGDATA)
        ti_append_target_property(${the_target} COMPILE_DEFINITIONS "TIGER_TEST_BIGDATA=1")
      endif()

      if(NOT BUILD_tiger_world)
        _ti_add_precompiled_headers(${the_target})
      endif()

      if(TIGER_TEST_EXTRA_CXX_FLAGS_Release)
        target_compile_options(${the_target} PRIVATE "$<$<CONFIG:Release>:${TIGER_TEST_EXTRA_CXX_FLAGS_Release}>")
      endif()

      ti_add_test_from_target("${the_target}" "Accuracy" "${the_target}")
    else(TI_DEPENDENCIES_FOUND)
      # TODO: warn about unsatisfied dependencies
    endif(TI_DEPENDENCIES_FOUND)

    if(INSTALL_TESTS)
      install(TARGETS ${the_target} RUNTIME DESTINATION ${TIGER_TEST_INSTALL_PATH} COMPONENT tests)
    endif()
  endif()
endfunction()

function(ti_add_samples)
  ti_debug_message("ti_add_samples(" ${ARGN} ")")

  set(samples_path "${CMAKE_CURRENT_SOURCE_DIR}/samples")
  if(NOT EXISTS "${samples_path}")
    return()
  endif()

  string(REGEX REPLACE "^tiger_" "" module_id ${the_module})

  if(BUILD_EXAMPLES)
    set(samples_deps ${the_module} ${TIGER_MODULE_${the_module}_DEPS} tiger_imgcodecs tiger_videoio tiger_highgui ${ARGN})
    ti_check_dependencies(${samples_deps})

    if(TI_DEPENDENCIES_FOUND)
      file(GLOB sample_sources "${samples_path}/*.cpp")

      foreach(source ${sample_sources})
        get_filename_component(name "${source}" NAME_WE)
        set(the_target "example_${module_id}_${name}")

        ti_add_executable(${the_target} "${source}")
        ti_target_include_modules(${the_target} ${samples_deps})
        ti_target_link_libraries(${the_target} PRIVATE ${samples_deps})

        set_target_properties(${the_target} PROPERTIES
          PROJECT_LABEL "(sample) ${name}"
          LABELS "${TIGER_MODULE_${the_module}_LABEL};Sample")
        set_source_files_properties("${source}" PROPERTIES
          LABELS "${TIGER_MODULE_${the_module}_LABEL};Sample")
        if(ENABLE_SOLUTION_FOLDERS)
          set_target_properties(${the_target} PROPERTIES
            FOLDER "samples/${module_id}")
        endif()
        # Add single target to build all samples for the module: 'make tiger_samples_bioinspired'
        set(parent_target tiger_samples_${module_id})
        if(NOT TARGET ${parent_target})
          add_custom_target(${parent_target})
          add_dependencies(tiger_samples ${parent_target})
        endif()
        add_dependencies(${parent_target} ${the_target})

        if(TARGET tiger_videoio_plugins)
          add_dependencies(${the_target} tiger_videoio_plugins)
        endif()

        if(INSTALL_BIN_EXAMPLES)
          install(TARGETS ${the_target} RUNTIME DESTINATION "${TIGER_SAMPLES_BIN_INSTALL_PATH}/${module_id}" COMPONENT samples)
        endif()
      endforeach()
    endif()
  endif()

  if(INSTALL_C_EXAMPLES)
    file(GLOB DEPLOY_FILES_AND_DIRS "${samples_path}/*")
    foreach(ITEM ${DEPLOY_FILES_AND_DIRS})
        IF( IS_DIRECTORY "${ITEM}" )
            LIST( APPEND sample_dirs "${ITEM}" )
        ELSE()
            LIST( APPEND sample_files "${ITEM}" )
        ENDIF()
    endforeach()
    install(FILES ${sample_files}
            DESTINATION "${TIGER_SAMPLES_SRC_INSTALL_PATH}/${module_id}"
            COMPONENT samples)
    install(DIRECTORY ${sample_dirs}
            DESTINATION "${TIGER_SAMPLES_SRC_INSTALL_PATH}/${module_id}"
            COMPONENT samples)
  endif()
endfunction()
