# ----------------------------------------------------------------------------
# Finalization: generate configuration-based files
# ----------------------------------------------------------------------------

ti_cmake_hook(PRE_FINALIZE)

# Generate platform-dependent and configuration-dependent headers
include(cmake/TigerGenHeaders.cmake)

# Generate TigerConfig.cmake and TigerConfig-version.cmake for cmake projects
include(cmake/TigerGenConfig.cmake)

# Generate environment setup file
if(INSTALL_TESTS AND TIGER_TEST_DATA_PATH)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/${TI_INTERNAL_NAME}_run_all_tests_windows.cmd.in"
                   "${CMAKE_BINARY_DIR}/win-install/${TI_INTERNAL_NAME}_run_all_tests.cmd" @ONLY)
    install(PROGRAMS "${CMAKE_BINARY_DIR}/win-install/${TI_INTERNAL_NAME}_run_all_tests.cmd"
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
status("General configuration for ${PROJECT_NAME} ${TIGER_VERSION}")
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
   AND NOT BUILD_INFO_SKIP_TIMESTAMP)
  string(TIMESTAMP TIGER_TIMESTAMP "" UTC)
  set(TIGER_TIMESTAMP
      "${TIGER_TIMESTAMP}"
      CACHE STRING "Timestamp of Tiger build configuration" FORCE)
endif()
if(TIGER_TIMESTAMP)
  status("    Timestamp:" ${TIGER_TIMESTAMP})
endif()
status("    Host:" ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_VERSION}
       ${CMAKE_HOST_SYSTEM_PROCESSOR})
if(CMAKE_CROSSCOMPILING)
  status("    Target:" ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_VERSION}
         ${CMAKE_SYSTEM_PROCESSOR})
endif()
status("    CMake:" ${CMAKE_VERSION})
status("    CMake generator:" ${CMAKE_GENERATOR})
status("    CMake build tool:" ${CMAKE_BUILD_TOOL})
if(MSVC)
  status("    MSVC:" ${MSVC_VERSION})
endif()
if(CMAKE_GENERATOR MATCHES Xcode)
  status("    Xcode:" ${XCODE_VERSION})
endif()
if(CMAKE_GENERATOR MATCHES "Xcode|Visual Studio|Multi-Config")
  status("    Configuration:" ${CMAKE_CONFIGURATION_TYPES})
else()
  status("    Configuration:" ${CMAKE_BUILD_TYPE})
endif()

# ========================= CPU code generation mode =========================
status("")
status("  CPU/HW features:")
status("    Baseline:" "${CPU_BASELINE_FINAL}")
if(NOT CPU_BASELINE STREQUAL CPU_BASELINE_FINAL)
  status("      requested:" "${CPU_BASELINE}")
endif()
if(CPU_BASELINE_REQUIRE)
  status("      required:" "${CPU_BASELINE_REQUIRE}")
endif()
if(CPU_BASELINE_DISABLE)
  status("      disabled:" "${CPU_BASELINE_DISABLE}")
endif()
if(CPU_DISPATCH_FINAL OR CPU_DISPATCH)
  status("    Dispatched code generation:" "${CPU_DISPATCH_FINAL}")
  if(NOT CPU_DISPATCH STREQUAL CPU_DISPATCH_FINAL)
    status("      requested:" "${CPU_DISPATCH}")
  endif()
  if(CPU_DISPATCH_REQUIRE)
    status("      required:" "${CPU_DISPATCH_REQUIRE}")
  endif()
  foreach(OPT ${CPU_DISPATCH_FINAL})
    status("      ${OPT} (${CPU_${OPT}_USAGE_COUNT} files):"
           "+ ${CPU_DISPATCH_${OPT}_INCLUDED}")
  endforeach()
endif()

# ========================== C/C++ options ==========================
if(CMAKE_CXX_COMPILER_VERSION)
  set(TIGER_COMPILER_STR
      "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1} (ver ${CMAKE_CXX_COMPILER_VERSION})"
  )
else()
  set(TIGER_COMPILER_STR "${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_ARG1}")
endif()
string(STRIP "${TIGER_COMPILER_STR}" TIGER_COMPILER_STR)

status("")
status("  C/C++:")
status("    Built as dynamic libs?:" BUILD_SHARED_LIBS THEN YES ELSE NO)
if(DEFINED CMAKE_CXX_STANDARD AND CMAKE_CXX_STANDARD)
  status("    C++ standard:" "${CMAKE_CXX_STANDARD}")
endif()
status("    C++ Compiler:" ${TIGER_COMPILER_STR})
status("    C++ flags (Release):" ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE})
status("    C++ flags (Debug):" ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG})
status("    C Compiler:" ${CMAKE_C_COMPILER} ${CMAKE_C_COMPILER_ARG1})
status("    C flags (Release):" ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE})
status("    C flags (Debug):" ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG})
if(WIN32)
  status("    Linker flags (Release):" ${CMAKE_EXE_LINKER_FLAGS}
         ${CMAKE_EXE_LINKER_FLAGS_RELEASE})
  status("    Linker flags (Debug):" ${CMAKE_EXE_LINKER_FLAGS}
         ${CMAKE_EXE_LINKER_FLAGS_DEBUG})
else()
  status("    Linker flags (Release):" ${CMAKE_SHARED_LINKER_FLAGS}
         ${CMAKE_SHARED_LINKER_FLAGS_RELEASE})
  status("    Linker flags (Debug):" ${CMAKE_SHARED_LINKER_FLAGS}
         ${CMAKE_SHARED_LINKER_FLAGS_DEBUG})
endif()
status("    ccache:" TIGER_COMPILER_IS_CCACHE THEN YES ELSE NO)
status(
  "    Precompiled headers:"
  PCHSupport_FOUND
  AND
  ENABLE_PRECOMPILED_HEADERS
  THEN
  YES
  ELSE
  NO)

# ========================== Dependencies ============================
ti_get_all_libs(deps_modules deps_extra deps_3rdparty)
status("    Extra dependencies:" ${deps_extra})
status("    3rdparty dependencies:" ${deps_3rdparty})

# ========================== Tiger modules ==========================
status("")
status("  Tiger modules:")
set(TIGER_MODULES_BUILD_ST "")
foreach(the_module ${TIGER_MODULES_BUILD})
  if(NOT TIGER_MODULE_${the_module}_CLASS STREQUAL "INTERNAL"
     OR the_module STREQUAL "${TI_INTERNAL_NAME}_ts")
    list(APPEND TIGER_MODULES_BUILD_ST "${the_module}")
  endif()
endforeach()
string(REPLACE "${TI_INTERNAL_NAME}_" "" TIGER_MODULES_BUILD_ST "${TIGER_MODULES_BUILD_ST}")
string(REPLACE "${TI_INTERNAL_NAME}_" "" TIGER_MODULES_DISABLED_USER_ST
               "${TIGER_MODULES_DISABLED_USER}")
string(REPLACE "${TI_INTERNAL_NAME}_" "" TIGER_MODULES_DISABLED_AUTO_ST
               "${TIGER_MODULES_DISABLED_AUTO}")
string(REPLACE "${TI_INTERNAL_NAME}_" "" TIGER_MODULES_DISABLED_FORCE_ST
               "${TIGER_MODULES_DISABLED_FORCE}")
list(SORT TIGER_MODULES_BUILD_ST)
list(SORT TIGER_MODULES_DISABLED_USER_ST)
list(SORT TIGER_MODULES_DISABLED_AUTO_ST)
list(SORT TIGER_MODULES_DISABLED_FORCE_ST)
status("    To be built:" TIGER_MODULES_BUILD THEN ${TIGER_MODULES_BUILD_ST}
       ELSE "-")
status("    Disabled:" TIGER_MODULES_DISABLED_USER THEN
       ${TIGER_MODULES_DISABLED_USER_ST} ELSE "-")
status("    Disabled by dependency:" TIGER_MODULES_DISABLED_AUTO THEN
       ${TIGER_MODULES_DISABLED_AUTO_ST} ELSE "-")
status("    Unavailable:" TIGER_MODULES_DISABLED_FORCE THEN
       ${TIGER_MODULES_DISABLED_FORCE_ST} ELSE "-")

ti_build_features_string(
  apps_status
  IF
  BUILD_TESTS
  AND
  HAVE_${TI_INTERNAL_NAME}_ts
  THEN
  "tests"
  IF
  BUILD_PERF_TESTS
  AND
  HAVE_${TI_INTERNAL_NAME}_ts
  THEN
  "perf_tests"
  IF
  BUILD_EXAMPLES
  THEN
  "examples"
  IF
  BUILD_${TI_INTERNAL_NAME}_apps
  THEN
  "apps"
  IF
  BUILD_ANDROID_SERVICE
  THEN
  "android_service"
  IF
  (BUILD_ANDROID_EXAMPLES OR INSTALL_ANDROID_EXAMPLES)
  AND
  CAN_BUILD_ANDROID_PROJECTS
  THEN
  "android_examples"
  ELSE
  "-")
status("    Applications:" "${apps_status}")
ti_build_features_string(
  docs_status
  IF
  TARGET
  doxygen_cpp
  THEN
  "doxygen"
  IF
  TARGET
  doxygen_python
  THEN
  "python"
  IF
  TARGET
  doxygen_javadoc
  THEN
  "javadoc"
  IF
  BUILD_${TI_INTERNAL_NAME}_js
  OR
  DEFINED
  TIGER_JS_LOCATION
  THEN
  "js"
  ELSE
  "NO")

# ================== Other third-party libraries ================
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