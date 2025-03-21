# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(
    FATAL_ERROR
      "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif()

#
# Configure CMake policies
#
if(POLICY CMP0026)
  cmake_policy(SET CMP0026 NEW)
endif()

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0+ (2.8.12): MacOS "@rpath" in
                                # target's install name
endif()

if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW) # warn about non-existed dependencies
endif()

if(POLICY CMP0051)
  cmake_policy(SET CMP0051 NEW)
endif()

if(POLICY CMP0054) # CMake 3.1: Only interpret if() arguments as variables or
                   # keywords when unquoted.
  cmake_policy(SET CMP0054 NEW)
endif()

if(POLICY CMP0056)
  cmake_policy(SET CMP0056 NEW) # try_compile(): link flags
endif()

if(POLICY CMP0066)
  cmake_policy(SET CMP0066 NEW) # CMake 3.7: try_compile(): use per-config
                                # flags, like CMAKE_CXX_FLAGS_RELEASE
endif()

if(POLICY CMP0067)
  cmake_policy(SET CMP0067 NEW) # CMake 3.8: try_compile(): honor language
                                # standard variables (like C++11)
endif()

if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW) # CMake 3.9+: `RPATH` settings on macOS do not
                                # affect `install_name`.
endif()

if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW) # CMake 3.12+: Include file check macros honor
                                # `CMAKE_REQUIRED_LIBRARIES`
endif()

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW) # CMake 3.13+: option() honors normal variables.
endif()

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW) # CMake 3.15+: MSVC runtime library flags are
                                # selected by an abstraction.
endif()

#
# Auto configuration
#
set(CMAKE_SAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples)

#
# Configure Tiger CMake hooks
#
include(cmake/TigerUtils.cmake)
include(cmake/TigerUtilsExtra.cmake)
ti_cmake_reset_hooks()
ti_check_environment_variables(TIGER_CMAKE_HOOKS_DIR)
if(DEFINED TIGER_CMAKE_HOOKS_DIR)
  foreach(__dir ${TIGER_CMAKE_HOOKS_DIR})
    get_filename_component(__dir "${__dir}" ABSOLUTE)
    ti_cmake_hook_register_dir(${__dir})
  endforeach()
endif()

ti_cmake_hook(CMAKE_INIT)

# must go before the project()/enable_language() commands
ti_update(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs"
          FORCE)
if(DEFINED CMAKE_BUILD_TYPE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
                                               "${CMAKE_CONFIGURATION_TYPES}")
endif()

option(ENABLE_PIC
       "Generate position independent code (necessary for shared libraries)"
       TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE ${ENABLE_PIC})

ti_cmake_hook(PRE_CMAKE_BOOTSTRAP)

# Bootstap CMake system: setup CMAKE_SYSTEM_NAME and other vars
if(TIGER_WORKAROUND_CMAKE_20989)
  set(CMAKE_SYSTEM_PROCESSOR_BACKUP ${CMAKE_SYSTEM_PROCESSOR})
endif()
if(TIGER_WORKAROUND_CMAKE_20989)
  set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR_BACKUP})
endif()

ti_cmake_hook(POST_CMAKE_BOOTSTRAP)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

  # https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT.html
  set(CMAKE_INSTALL_PREFIX
      "${CMAKE_BINARY_DIR}/install"
      CACHE PATH "Installation Directory" FORCE)
endif()

ti_get_date_components(TI_UPDATE_YEAR TI_UPDATE_MONTH)

enable_testing(true)
