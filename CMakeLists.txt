# ----------------------------------------------------------------------------
# Copyright (c) 2021
# SPDX-License-Identifier: MIT
#
#  This file is the Root CMake file for Tiger Template.
# Distributed under the MIT License. See LICENSE file for details.
#
#    From the off-tree build directory, invoke:
#      $ cmake <PATH_TO_TIGER_ROOT>
#
# Created Date: 2021-4-12
#
# ----------------------------------------------------------------------------

# Disable in-source builds to prevent source tree corruption.
if(" ${CMAKE_SOURCE_DIR}" STREQUAL " ${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "
FATAL: In-source builds are not allowed.
       You should create a separate directory for build files.
")
endif()

include(cmake/TigerMinDepVersions.cmake)
cmake_minimum_required(VERSION "${MIN_VER_CMAKE}" FATAL_ERROR)

#
# Configure CMake policies
#
if(POLICY CMP0026)
  cmake_policy(SET CMP0026 NEW)
endif()

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)  # CMake 3.0+ (2.8.12): MacOS "@rpath" in target's install name
endif()

if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)  # warn about non-existed dependencies
endif()

if(POLICY CMP0051)
  cmake_policy(SET CMP0051 NEW)
endif()

if(POLICY CMP0054)  # CMake 3.1: Only interpret if() arguments as variables or keywords when unquoted.
  cmake_policy(SET CMP0054 NEW)
endif()

if(POLICY CMP0056)
  cmake_policy(SET CMP0056 NEW)  # try_compile(): link flags
endif()

if(POLICY CMP0066)
  cmake_policy(SET CMP0066 NEW)  # CMake 3.7: try_compile(): use per-config flags, like CMAKE_CXX_FLAGS_RELEASE
endif()

if(POLICY CMP0067)
  cmake_policy(SET CMP0067 NEW)  # CMake 3.8: try_compile(): honor language standard variables (like C++11)
endif()

if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)  # CMake 3.9+: `RPATH` settings on macOS do not affect `install_name`.
endif()

if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)  # CMake 3.12+: Include file check macros honor `CMAKE_REQUIRED_LIBRARIES`
endif()

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)  # CMake 3.13+: option() honors normal variables.
endif()

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)  # CMake 3.15+: MSVC runtime library flags are selected by an abstraction.
endif()
#
# Auto configuration
#
SET(CMAKE_SAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/samples)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/project_config.h.in
	${CMAKE_CURRENT_SOURCE_DIR}/modules/base/include/tiger/base/project_config.h @ONLY
)

#
# Configure Tiger CMake hooks
#
include(cmake/TigerUtils.cmake)
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
ti_update(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Configs" FORCE)
if(DEFINED CMAKE_BUILD_TYPE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "${CMAKE_CONFIGURATION_TYPES}")
endif()

option(ENABLE_PIC "Generate position independent code (necessary for shared libraries)" TRUE)
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

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)  # https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT.html
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory" FORCE)
endif()

enable_testing(true)

project(Tiger CXX C)
enable_language(CXX C)

set(CMAKE_USE_RELATIVE_PATHS ON CACHE INTERNAL "" FORCE)

ti_cmake_eval(DEBUG_PRE ONCE)

ti_clear_vars(TigerModules_TARGETS)

include(cmake/TigerDownload.cmake)

set(BUILD_LIST "" CACHE STRING "Build only listed modules (comma-separated, e.g. 'base,network,laser,...')")
set(BUILD_APP_LIST "" CACHE STRING "Build only listed applications (comma-separated, e.g. 'tiger,tester1,...')")

# ----------------------------------------------------------------------------
# Break in case of popular CMake configuration mistakes
# ----------------------------------------------------------------------------
if(NOT CMAKE_SIZEOF_VOID_P GREATER 0)
  message(FATAL_ERROR "CMake fails to determine the bitness of the target platform.
  Please check your CMake and compiler installation. If you are cross-compiling then ensure that your CMake toolchain file correctly sets the compiler details.")
endif()

# ----------------------------------------------------------------------------
# Detect compiler and target platform architecture
# ----------------------------------------------------------------------------
include(cmake/TigerDetectCXXCompiler.cmake)
ti_cmake_hook(POST_DETECT_COMPILER)

# ----------------------------------------------------------------------------
# Tiger cmake options
# ----------------------------------------------------------------------------

# 3rd party libs


# Optional 3rd party components
# ===================================================


# Tiger build components
# ===================================================
TI_OPTION(BUILD_SHARED_LIBS        "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" ON )
TI_OPTION(BUILD_TIGER_APPS        "Build utility applications (used for example or test)" ON )
TI_OPTION(BUILD_TESTS           "Build all tests"                          OFF )
set(ENABLE_PRECOMPILED_HEADERS 1) #Use precompiled headers 

# Tiger installation options
# ===================================================
TI_OPTION(INSTALL_CREATE_DISTRIB   "Change install rules to build the distribution package" OFF )
TI_OPTION(INSTALL_TESTS            "Install accuracy and performance test binaries and test data" OFF )

# Tiger build options
# ===================================================
TI_OPTION(TIGER_ENABLE_MEMORY_SANITIZER "Better support for memory/address sanitizers"         OFF)
TI_OPTION(TIGER_WARNINGS_ARE_ERRORS "Treat warnings as errors"                                 OFF )
TI_OPTION(TIGER_ENABLE_MEMALIGN     "Enable posix_memalign or memalign usage" ON )
TI_OPTION(ENABLE_CONFIG_VERIFICATION "Fail build if actual configuration doesn't match requested (WITH_XXX != HAVE_XXX)" OFF )
TI_OPTION(ENABLE_SOLUTION_FOLDERS    "Solution folder in Visual Studio"        ON )

# ----------------------------------------------------------------------------
#  Get actual Tiger version number from sources
# ----------------------------------------------------------------------------
include(cmake/TigerVersion.cmake)

ti_cmake_hook(POST_OPTIONS)

# ----------------------------------------------------------------------------
#  Build & install layouts
# ----------------------------------------------------------------------------

if(TIGER_TEST_DATA_PATH)
  get_filename_component(TIGER_TEST_DATA_PATH ${TIGER_TEST_DATA_PATH} ABSOLUTE)
endif()

# Save libs and executables in the same place
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_BINARY_DIR}/bin" CACHE PATH "Output directory for applications")

set(LIBRARY_OUTPUT_PATH                "${Tiger_BINARY_DIR}/lib")
ti_update(3P_LIBRARY_OUTPUT_PATH      "${Tiger_BINARY_DIR}/3rdparty/lib")

# Postfix of DLLs:
ti_update(TIGER_DLLVERSION "${TIGER_VERSION_MAJOR}${TIGER_VERSION_MINOR}${TIGER_VERSION_PATCH}")
ti_update(TIGER_DEBUG_POSTFIX d)

if(DEFINED CMAKE_DEBUG_POSTFIX)
  set(TIGER_DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
endif()

include(cmake/TigerInstallLayout.cmake)

# ----------------------------------------------------------------------------
#  Path for build/platform -specific headers
# ----------------------------------------------------------------------------
ti_update(TIGER_CONFIG_FILE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/" CACHE PATH "Where to create the platform-dependant ticonfig.h")
ti_include_directories(${TIGER_CONFIG_FILE_INCLUDE_DIR})

# ----------------------------------------------------------------------------
#  Path for additional modules
# ----------------------------------------------------------------------------
set(TIGER_EXTRA_MODULES_PATH "" CACHE PATH "Where to look for additional Tiger modules (can be ;-separated list of paths)")

# ----------------------------------------------------------------------------
#  Autodetect if we are in a GIT repository
# ----------------------------------------------------------------------------
find_host_package(Git QUIET)

if(NOT DEFINED TIGER_VCSVERSION AND GIT_FOUND)
  ti_git_describe(TIGER_VCSVERSION "${Tiger_SOURCE_DIR}")
elseif(NOT DEFINED TIGER_VCSVERSION)
  # We don't have git:
  set(TIGER_VCSVERSION "unknown")
endif()


# ----------------------------------------------------------------------------
# Tiger compiler and linker options
# ----------------------------------------------------------------------------
# In case of Makefiles if the user does not setup CMAKE_BUILD_TYPE, assume it's Release:
if(CMAKE_GENERATOR MATCHES "Makefiles|Ninja" AND "${CMAKE_BUILD_TYPE}" STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

ti_cmake_hook(POST_CMAKE_BUILD_OPTIONS)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include(cmake/TigerCompilerOptions.cmake)

ti_cmake_hook(POST_COMPILER_OPTIONS)

# ----------------------------------------------------------------------------
#       CHECK FOR SYSTEM LIBRARIES, OPTIONS, ETC..
# ----------------------------------------------------------------------------
include(CheckIncludeFile)
include(CheckSymbolExists)

if(TIGER_ENABLE_MEMALIGN)
    CHECK_INCLUDE_FILE(malloc.h HAVE_MALLOC_H)
    if(HAVE_MALLOC_H)
        CHECK_SYMBOL_EXISTS(_aligned_malloc malloc.h HAVE_WIN32_ALIGNED_MALLOC)
    endif()
endif()

include(cmake/TigerPCHSupport.cmake)
include(cmake/TigerModule.cmake)
include(cmake/TigerApp.cmake)

# ----------------------------------------------------------------------------
#  Detect endianness of build platform
# ----------------------------------------------------------------------------
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

# ----------------------------------------------------------------------------
#  Detect 3rd-party libraries
# ----------------------------------------------------------------------------
find_package(OpenGL REQUIRED)

# Set vcpkg as the toolchain
set(VCPKG_ROOT "/home/get/tools/vcpkg")
set(VCPKG_ROOT CACHE PATH "Vcpkg Root Directory")

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(EXISTS ${CMAKE_TOOLCHAIN_FILE})
include(${CMAKE_TOOLCHAIN_FILE})
else()
    message(FATAL_ERROR "vcpkg toolchain file not found: ${CMAKE_TOOLCHAIN_FILE}")
endif()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM "NEVER") # Find programs (compiler) in system paths ONLY
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY "ONLY") # Find libraries ONLY in vcpkg
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE "ONLY")# Find includes ONLY in vcpkg

# These are still important to prevent Windows path issues in WSL:
set(CMAKE_FIND_USE_SYSTEM_ENVIRONMENT_PATH "OFF")
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH "OFF")

# set(CMAKE_TOOLCHAIN_FILE "${vcpkg_SOURCE_DIR}/scripts/buildsystems/vcpkg.cmake")
# set(VCPKG_ROOT "${vcpkg_SOURCE_DIR}")

# include(cmake/TigerFindDevTools.cmake)
# include(cmake/TigerFindCameraDrivers.cmake)
# include(cmake/TigerFindAxisDrivers.cmake)
include(cmake/TigerFindProtobuf.cmake)
include(cmake/TigerFindQt.cmake)
# include(cmake/TigerFindOpenCV.cmake)
# include(cmake/TigerFindBoost.cmake)
# include(cmake/TigerFindVTK.cmake)
include(cmake/TigerFindlog4cplus.cmake)


# ----------------------------------------------------------------------------
#  Detect other 3rd-party libraries/tools
# ----------------------------------------------------------------------------

ti_cmake_hook(POST_DETECT_DEPENDECIES)  # typo, deprecated (2019-06)
ti_cmake_hook(POST_DETECT_DEPENDENCIES)

# ----------------------------------------------------------------------------
# Solution folders:
# ----------------------------------------------------------------------------
if(ENABLE_SOLUTION_FOLDERS)
  set_property(GLOBAL PROPERTY USE_FOLDERS ON)
  set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
endif()

# Extra Tiger targets: uninstall, package_source, perf, etc.
include(cmake/TigerExtraTargets.cmake)

# ----------------------------------------------------------------------------
# Process subdirectories
# ----------------------------------------------------------------------------

# Enable compiler options for Tiger modules/apps/samples only (ignore 3rdparty)
ti_add_modules_compiler_options()

# proto
add_subdirectory(proto)

# Tiger modules
add_subdirectory(modules)

# extra applications
if(BUILD_TIGER_APPS)
  add_subdirectory(apps)
endif()

# ----------------------------------------------------------------------------
# Finalization: generate configuration-based files
# ----------------------------------------------------------------------------

ti_cmake_hook(PRE_FINALIZE)

# Generate platform-dependent and configuration-dependent headers
include(cmake/TigerGenHeaders.cmake)


# Generate environment setup file
if(INSTALL_TESTS AND TIGER_TEST_DATA_PATH)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/tiger_run_all_tests_windows.cmd.in"
                   "${CMAKE_BINARY_DIR}/win-install/tiger_run_all_tests.cmd" @ONLY)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/win-install/tiger_run_all_tests.cmd"
            DESTINATION ${TIGER_TEST_INSTALL_PATH} COMPONENT tests)
endif()

if(NOT TIGER_LICENSE_FILE)
  set(TIGER_LICENSE_FILE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
endif()

# it does not make sense as LICENSE and readme will be part of the package automatically
install(FILES ${TIGER_LICENSE_FILE}
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DESTINATION ./ COMPONENT libs)
if(TIGER_README_FILE)
install(FILES ${TIGER_README_FILE}
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DESTINATION ./ COMPONENT libs)
endif()

if(TIGER_GENERATE_SETUPVARS)
  include(cmake/TigerGenSetupVars.cmake)
endif()

# ----------------------------------------------------------------------------
# Summary:
# ----------------------------------------------------------------------------
status("")
status("General configuration for Tiger ${TIGER_VERSION}")
status("=====================================")
if(TIGER_VCSVERSION)
  status("  Version control:" ${TIGER_VCSVERSION})
endif()
if(TIGER_EXTRA_MODULES_PATH AND NOT BUILD_INFO_SKIP_EXTRA_MODULES)
  set(__dump_extra_header OFF)
  foreach(p ${TIGER_EXTRA_MODULES_PATH})
    if(EXISTS ${p})
      if(NOT __dump_extra_header)
        set(__dump_extra_header ON)
        status("")
        status("  Extra modules:")
      else()
        status("")
      endif()
      ti_git_describe(EXTRA_MODULES_VCSVERSION "${p}")
      status("    Location (extra):" ${p})
      status("    Version control (extra):" ${EXTRA_MODULES_VCSVERSION})
    endif()
  endforeach()
  unset(__dump_extra_header)
endif()

# ========================== build platform ==========================
status("")
status("  Platform:")
if(NOT DEFINED TIGER_TIMESTAMP
    AND NOT CMAKE_VERSION VERSION_LESS 2.8.11
    AND NOT BUILD_INFO_SKIP_TIMESTAMP
)
  string(TIMESTAMP TIGER_TIMESTAMP "" UTC)
  set(TIGER_TIMESTAMP "${TIGER_TIMESTAMP}" CACHE STRING "Timestamp of Tiger build configuration" FORCE)
endif()
if(TIGER_TIMESTAMP)
  status("    Timestamp:"      ${TIGER_TIMESTAMP})
endif()
status("    Host:"             ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION} ${CMAKE_HOST_SYSTEM_PROCESSOR})
if(CMAKE_CROSSCOMPILING)
  status("    Target:"         ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION} ${CMAKE_SYSTEM_PROCESSOR})
endif()
status("    CMake:"            ${CMAKE_VERSION})
status("    CMake generator:"  ${CMAKE_GENERATOR})
status("    CMake build tool:" ${CMAKE_BUILD_TOOL})
if(MSVC)
  status("    MSVC:"           ${MSVC_VERSION})
endif()
if(CMAKE_GENERATOR MATCHES Xcode)
  status("    Xcode:"          ${XCODE_VERSION})
endif()
if(CMAKE_GENERATOR MATCHES "Xcode|Visual Studio|Multi-Config")
  status("    Configuration:"  ${CMAKE_CONFIGURATION_TYPES})
else()
  status("    Configuration:"  ${CMAKE_BUILD_TYPE})
endif()


# ========================= CPU code generation mode =========================
status("")
status("  CPU/HW features:")
status("    Baseline:"  "${CPU_BASELINE_FINAL}")
if(NOT CPU_BASELINE STREQUAL CPU_BASELINE_FINAL)
  status("      requested:"  "${CPU_BASELINE}")
endif()
if(CPU_BASELINE_REQUIRE)
  status("      required:"  "${CPU_BASELINE_REQUIRE}")
endif()
if(CPU_BASELINE_DISABLE)
  status("      disabled:"  "${CPU_BASELINE_DISABLE}")
endif()
if(CPU_DISPATCH_FINAL OR CPU_DISPATCH)
  status("    Dispatched code generation:"  "${CPU_DISPATCH_FINAL}")
  if(NOT CPU_DISPATCH STREQUAL CPU_DISPATCH_FINAL)
    status("      requested:"  "${CPU_DISPATCH}")
  endif()
  if(CPU_DISPATCH_REQUIRE)
    status("      required:"  "${CPU_DISPATCH_REQUIRE}")
  endif()
  foreach(OPT ${CPU_DISPATCH_FINAL})
    status("      ${OPT} (${CPU_${OPT}_USAGE_COUNT} files):"  "+ ${CPU_DISPATCH_${OPT}_INCLUDED}")
  endforeach()
endif()

# ========================== C/C++ options ==========================
if(CMAKE_CXX_COMPILER_VERSION)
  set(TIGER_COMPILER_STR "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} (ver ${CMAKE_CXX_COMPILER_VERSION})")
else()
  set(TIGER_COMPILER_STR "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1}")
endif()
string(STRIP "${TIGER_COMPILER_STR}" TIGER_COMPILER_STR)

status("")
status("  C/C++:")
status("    Built as dynamic libs?:" BUILD_SHARED_LIBS THEN YES ELSE NO)
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD)
  status("    C++ standard:"           "${CMAKE_CXX_STANDARD}")
endif()
status("    C++ Compiler:"           ${TIGER_COMPILER_STR})
status("    C++ flags (Release):"    ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE})
status("    C++ flags (Debug):"      ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG})
status("    C Compiler:"             ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1})
status("    C flags (Release):"      ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE})
status("    C flags (Debug):"        ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG})
if(WIN32)
  status("    Linker flags (Release):" ${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
  status("    Linker flags (Debug):"   ${CMAKE_EXE_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
else()
  status("    Linker flags (Release):" ${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
  status("    Linker flags (Debug):"   ${CMAKE_SHARED_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
endif()
status("    ccache:"                  TIGER_COMPILER_IS_CCACHE THEN YES ELSE NO)
status("    Precompiled headers:"     PCHSupport_FOUND AND ENABLE_PRECOMPILED_HEADERS THEN YES ELSE NO)

# ========================== Dependencies ============================
ti_get_all_libs(deps_modules deps_extra deps_3rdparty)
status("    Extra dependencies:" ${deps_extra})
status("    3rdparty dependencies:" ${deps_3rdparty})

# ========================== Tiger modules ==========================
status("")
status("  Tiger modules:")
set(TIGER_MODULES_BUILD_ST "")
foreach(the_module ${TIGER_MODULES_BUILD})
  if(NOT TIGER_MODULE_${the_module}_CLASS STREQUAL "INTERNAL" OR the_module STREQUAL "tiger_ts")
    list(APPEND TIGER_MODULES_BUILD_ST "${the_module}")
  endif()
endforeach()
string(REPLACE "tiger_" "" TIGER_MODULES_BUILD_ST          "${TIGER_MODULES_BUILD_ST}")
string(REPLACE "tiger_" "" TIGER_MODULES_DISABLED_USER_ST  "${TIGER_MODULES_DISABLED_USER}")
string(REPLACE "tiger_" "" TIGER_MODULES_DISABLED_AUTO_ST  "${TIGER_MODULES_DISABLED_AUTO}")
string(REPLACE "tiger_" "" TIGER_MODULES_DISABLED_FORCE_ST "${TIGER_MODULES_DISABLED_FORCE}")
list(SORT TIGER_MODULES_BUILD_ST)
list(SORT TIGER_MODULES_DISABLED_USER_ST)
list(SORT TIGER_MODULES_DISABLED_AUTO_ST)
list(SORT TIGER_MODULES_DISABLED_FORCE_ST)
status("    To be built:"            TIGER_MODULES_BUILD          THEN ${TIGER_MODULES_BUILD_ST}          ELSE "-")
status("    Disabled:"               TIGER_MODULES_DISABLED_USER  THEN ${TIGER_MODULES_DISABLED_USER_ST}  ELSE "-")
status("    Disabled by dependency:" TIGER_MODULES_DISABLED_AUTO  THEN ${TIGER_MODULES_DISABLED_AUTO_ST}  ELSE "-")
status("    Unavailable:"            TIGER_MODULES_DISABLED_FORCE THEN ${TIGER_MODULES_DISABLED_FORCE_ST} ELSE "-")

ti_build_features_string(apps_status
  IF BUILD_TESTS AND HAVE_tiger_ts THEN "tests"
  IF BUILD_PERF_TESTS AND HAVE_tiger_ts THEN "perf_tests"
  IF BUILD_EXAMPLES THEN "examples"
  IF BUILD_tiger_apps THEN "apps"
  IF BUILD_ANDROID_SERVICE THEN "android_service"
  IF (BUILD_ANDROID_EXAMPLES OR INSTALL_ANDROID_EXAMPLES) AND CAN_BUILD_ANDROID_PROJECTS THEN "android_examples"
  ELSE "-")
status("    Applications:" "${apps_status}")
ti_build_features_string(docs_status
    IF TARGET doxygen_cpp THEN "doxygen"
    IF TARGET doxygen_python THEN "python"
    IF TARGET doxygen_javadoc THEN "javadoc"
    IF BUILD_tiger_js OR DEFINED TIGER_JS_LOCATION THEN "js"
    ELSE "NO"
)

# ========================== Other third-party libraries ==========================
status("")
status("  Other third-party libraries:")

foreach(s ${CUSTOM_STATUS})
  status(${CUSTOM_STATUS_${s}})
endforeach()


# ========================== auxiliary ==========================
status("")
status("  Install to:" "${CMAKE_INSTALL_PREFIX}")
status("-----------------------------------------------------------------")
status("")


ti_finalize_status()

if(ENABLE_CONFIG_VERIFICATION)
  ti_verify_config()
endif()

ti_cmake_hook(POST_FINALIZE)

# ----------------------------------------------------------------------------
# CPack stuff
# ----------------------------------------------------------------------------

include(cmake/TigerPackaging.cmake)

# This should be the last command
ti_cmake_dump_vars("" TOFILE "CMakeVars.txt")
ti_cmake_eval(DEBUG_POST ONCE)
