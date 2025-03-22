string(TOLOWER "${TI_PROJECT_NAME}" TI_INTERNAL_NAME)

include(cmake/TigerUtilsExtend.cmake)
include(cmake/TigerMainAutoConfig.cmake)

set(CMAKE_USE_RELATIVE_PATHS
    ON
    CACHE INTERNAL "" FORCE)

ti_cmake_eval(DEBUG_PRE ONCE)

ti_clear_vars(TigerModules_TARGETS)

include(cmake/TigerDownload.cmake)

set(BUILD_LIST
    ""
    CACHE STRING
          "Build only listed modules (comma-separated, e.g. 'base,sim,opt,...')"
)
set(BUILD_APP_LIST
    ""
    CACHE
      STRING
      "Build only listed applications (comma-separated, e.g. 'example,tester,...')"
)

# ----------------------------------------------------------------------------
# Break in case of popular CMake configuration mistakes
# ----------------------------------------------------------------------------
if(NOT CMAKE_SIZEOF_VOID_P GREATER 0)
  message(
    FATAL_ERROR
      "CMake fails to determine the bitness of the target platform.
  Please check your CMake and compiler installation. If you are cross-compiling then ensure that your CMake toolchain file correctly sets the compiler details."
  )
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
ti_option(BUILD_SHARED_LIBS
          "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)"
          ON)
ti_option(BUILD_TIGER_APPS
          "Build utility applications (used for example or test)" ON)
ti_option(BUILD_TESTS "Build all tests" OFF)
set(ENABLE_PRECOMPILED_HEADERS 1) # Use precompiled headers

# Tiger installation options
# ===================================================
ti_option(INSTALL_CREATE_DISTRIB
          "Change install rules to build the distribution package" OFF)
ti_option(INSTALL_TESTS
          "Install accuracy and performance test binaries and test data" OFF)

# Tiger build options
# ===================================================
ti_option(TIGER_ENABLE_MEMORY_SANITIZER
          "Better support for memory/address sanitizers" OFF)
ti_option(TIGER_WARNINGS_ARE_ERRORS "Treat warnings as errors" OFF)
ti_option(TIGER_ENABLE_MEMALIGN "Enable posix_memalign or memalign usage" ON)
ti_option(
  ENABLE_CONFIG_VERIFICATION
  "Fail build if actual configuration doesn't match requested (WITH_XXX != HAVE_XXX)"
  OFF)
ti_option(ENABLE_SOLUTION_FOLDERS "Solution folder in Visual Studio" ON)
ti_option(TIGER_CMAKE_DEBUG_MESSAGES
          "Enable debug messages for Tiger CMake scripts" OFF)

# ----------------------------------------------------------------------------
# Get actual Tiger version number from sources
# ----------------------------------------------------------------------------
include(cmake/TigerVersion.cmake)

ti_cmake_hook(POST_OPTIONS)

# ----------------------------------------------------------------------------
# Build & install layouts
# ----------------------------------------------------------------------------

if(TIGER_TEST_DATA_PATH)
  get_filename_component(TIGER_TEST_DATA_PATH ${TIGER_TEST_DATA_PATH} ABSOLUTE)
endif()

# Save libs and executables in the same place
set(EXECUTABLE_OUTPUT_PATH
    "${CMAKE_BINARY_DIR}/bin"
    CACHE PATH "Output directory for applications")

set(LIBRARY_OUTPUT_PATH "${${PROJECT_NAME}_BINARY_DIR}/lib")
ti_update(3P_LIBRARY_OUTPUT_PATH "${${PROJECT_NAME}_BINARY_DIR}/3rdparty/lib")

# Postfix of DLLs:
ti_update(TIGER_DLLVERSION
          "${TIGER_VERSION_MAJOR}${TIGER_VERSION_MINOR}${TIGER_VERSION_PATCH}")
ti_update(TIGER_DEBUG_POSTFIX d)

if(DEFINED CMAKE_DEBUG_POSTFIX)
  set(TIGER_DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
endif()

include(cmake/TigerInstallLayout.cmake)

# ----------------------------------------------------------------------------
# Path for build/platform -specific headers
# ----------------------------------------------------------------------------
ti_update(TIGER_CONFIG_FILE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/" CACHE PATH
          "Where to create the platform-dependant ti_config.h")
ti_include_directories(${TIGER_CONFIG_FILE_INCLUDE_DIR})

# ----------------------------------------------------------------------------
# Path for additional modules
# ----------------------------------------------------------------------------
set(TIGER_EXTRA_MODULES_PATH
    ""
    CACHE
      PATH
      "Where to look for additional Tiger modules (can be ;-separated list of paths)"
)

# ----------------------------------------------------------------------------
# Autodetect if we are in a GIT repository
# ----------------------------------------------------------------------------
find_host_package(Git QUIET)

if(NOT DEFINED TIGER_VCSVERSION AND GIT_FOUND)
  ti_git_describe(TIGER_VCSVERSION "${${PROJECT_NAME}_SOURCE_DIR}")
elseif(NOT DEFINED TIGER_VCSVERSION)
  # We don't have git:
  set(TIGER_VCSVERSION "unknown")
endif()

# ----------------------------------------------------------------------------
# Tiger compiler and linker options
# ----------------------------------------------------------------------------
# In case of Makefiles if the user does not setup CMAKE_BUILD_TYPE, assume it's
# Release:
if(CMAKE_GENERATOR MATCHES "Makefiles|Ninja" AND "${CMAKE_BUILD_TYPE}" STREQUAL
                                                 "")
  set(CMAKE_BUILD_TYPE Release)
endif()

ti_cmake_hook(POST_CMAKE_BUILD_OPTIONS)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
include(cmake/TigerCompilerOptions.cmake)

ti_cmake_hook(POST_COMPILER_OPTIONS)

# ----------------------------------------------------------------------------
# CHECK FOR SYSTEM LIBRARIES, OPTIONS, ETC..
# ----------------------------------------------------------------------------
include(CheckIncludeFile)
include(CheckSymbolExists)

if(TIGER_ENABLE_MEMALIGN)
  check_include_file(malloc.h HAVE_MALLOC_H)
  if(HAVE_MALLOC_H)
    check_symbol_exists(_aligned_malloc malloc.h HAVE_WIN32_ALIGNED_MALLOC)
  endif()
endif()

include(cmake/TigerPCHSupport.cmake)
include(cmake/TigerModule.cmake)
include(cmake/TigerApp.cmake)

# ----------------------------------------------------------------------------
# Detect endianness of build platform
# ----------------------------------------------------------------------------
include(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

# ----------------------------------------------------------------------------
# Detect 3rd-party libraries
# ----------------------------------------------------------------------------
# find_package(OpenGL REQUIRED)

include(cmake/TigerFindProtobuf.cmake)
# include(cmake/TigerFindQt.cmake) include(cmake/TigerFindOpenCV.cmake)
# include(cmake/TigerFindBoost.cmake) include(cmake/TigerFindVTK.cmake)
# include(cmake/TigerFindlog4cplus.cmake)

# ----------------------------------------------------------------------------
# Detect other 3rd-party libraries/tools
# ----------------------------------------------------------------------------

ti_cmake_hook(POST_DETECT_DEPENDECIES) # typo, deprecated (2019-06)
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
ti_assemble_modules()

# Tiger applications
if(BUILD_TIGER_APPS)
  ti_assemble_applications()
endif()
