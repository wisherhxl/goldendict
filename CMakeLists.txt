# ----------------------------------------------------------------------------
# Copyright (c) 2021 SPDX-License-Identifier: MIT
#
# This file is the root CMake file for GoldenDict. The reusable project
# structure is provided by Tiger under MIT. See LICENSES/README.md.
#
# This file is intended to be used with Conan and CMake, Please refer to
# TigerPlatformUsage.md for more information.
#
# Created Date: 2021-4-12
#
# ----------------------------------------------------------------------------

# ============ Set Minimal CMake Version ============
# define MIN_VER_CMAKE if you want to set a version other than the default
include(cmake/TigerMinDepVersions.cmake)
cmake_minimum_required(VERSION "${MIN_VER_CMAKE}" FATAL_ERROR)

# ============ Load Policy and Utils ============
include(cmake/TigerMainPre.cmake)
# Sync Version
ti_sync_version()

# ============ Project Name ============
# Name of organization.
set(TI_ORG_NAME "")
# Email of project maintainer.
set(TI_PROJECT_CONTACT "")
# Name of the project.
set(TI_PROJECT_NAME "GoldenDict")
# Namespace of the project.
# Leave empty to use TI_PROJECT_NAME automatically.
set(TI_PROJECT_NAMESPACE "GoldenDict")
# Description of the project
set(TI_PROJECT_INFO "A feature-rich dictionary lookup program")

# ============ Project Definition ============
if(TI_PROJECT_NAME STREQUAL "")
  message(
    FATAL_ERROR
      "TI_PROJECT_NAME is not set.\n"
      "This template will not generate platform files until the project name is configured.\n"
      "Please edit CMakeLists.txt and set TI_PROJECT_NAME first. You can also set TI_PROJECT_NAMESPACE there if needed.")
endif()
project(${TI_PROJECT_NAME} CXX C)
enable_language(CXX C)

# ============ Modules Information ============
# Base module directory name for Tiger platform generated files.
# Leave empty to default to TI_INTERNAL_NAME after
# TigerMainSettings.cmake normalizes project naming variables.
set(TI_BASE_MODULE "tiger")
# Module directory name for generated protobuf files.
# Leave empty to use TI_BASE_MODULE automatically.
set(TI_PROTO_GEN_MODULE "")
# TI_FIXED_ORDER_MODULES: Modules listed here are built first, in the order
# specified. TI_BASE_MODULE is always prepended automatically.
set(TI_FIXED_ORDER_MODULES "")

# ============ Applications Information ============

# ============ Finalize Settings ============
include(cmake/TigerMainSettings.cmake)
include(cmake/TigerMainFinalize.cmake)

# OpenCC conversion data remains upstream-owned and is deployed verbatim from
# the resolved Conan package. The product behavior that consumes it is a later
# leaf, but keeping the payload here makes its release layout deterministic.
get_property(opencc_package_directory GLOBAL PROPERTY
  GOLDENDICT_OPENCC_PACKAGE_DIRECTORY)
if(NOT opencc_package_directory)
  message(FATAL_ERROR "OpenCC package deployment metadata is unavailable")
endif()
set(opencc_data_directory "${opencc_package_directory}/share/opencc")
set(opencc_runtime_data
  CJK_Compatibility_Ideographs.ocd2
  HKVariants.ocd2
  HKVariantsPhrases.ocd2
  STCharacters.ocd2
  STPhrases.ocd2
  STPhrases_GeneratedFromRegionalPhrases.ocd2
  TSCharacters.ocd2
  TSCharactersExt.ocd2
  TSPhrases.ocd2
  TWVariants.ocd2
  TWVariantsPhrases.ocd2
  s2hk.json
  s2tw.json
  t2s.json)
file(GLOB opencc_packaged_data
  RELATIVE "${opencc_data_directory}" "${opencc_data_directory}/*")
list(SORT opencc_runtime_data)
list(SORT opencc_packaged_data)
if(NOT "${opencc_packaged_data}" STREQUAL "${opencc_runtime_data}")
  message(FATAL_ERROR
    "The OpenCC Conan package does not contain the accepted runtime-data closure")
endif()
list(TRANSFORM opencc_runtime_data
  PREPEND "${opencc_data_directory}/"
  OUTPUT_VARIABLE opencc_runtime_data_paths)

set(opencc_license_files
  "${opencc_package_directory}/licenses/opencc/LICENSE"
  "${opencc_package_directory}/licenses/marisa-trie/COPYING.md"
  "${opencc_package_directory}/licenses/darts-clone/COPYING.md"
  "${opencc_package_directory}/licenses/rapidjson/rapidjson-license.txt")
foreach(opencc_license_file IN LISTS opencc_license_files)
  if(NOT EXISTS "${opencc_license_file}")
    message(FATAL_ERROR
      "The OpenCC Conan package is missing a required notice: ${opencc_license_file}")
  endif()
endforeach()

add_custom_target(goldendict_opencc_runtime_data ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:goldendict>/../share/goldendict/opencc"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${opencc_runtime_data_paths}
    "$<TARGET_FILE_DIR:goldendict>/../share/goldendict/opencc"
  VERBATIM)
add_dependencies(goldendict goldendict_opencc_runtime_data)
install(FILES ${opencc_runtime_data_paths}
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/goldendict/opencc"
  COMPONENT libs)
install(FILES "${opencc_package_directory}/licenses/opencc/LICENSE"
  DESTINATION "licenses/opencc" COMPONENT licenses)
install(FILES "${opencc_package_directory}/licenses/marisa-trie/COPYING.md"
  DESTINATION "licenses/marisa-trie" COMPONENT licenses)
install(FILES "${opencc_package_directory}/licenses/darts-clone/COPYING.md"
  DESTINATION "licenses/darts-clone" COMPONENT licenses)
install(FILES
  "${opencc_package_directory}/licenses/rapidjson/rapidjson-license.txt"
  DESTINATION "licenses/rapidjson" COMPONENT licenses)
if(TIGER_INSTALL_RUNTIME_DEPENDENCIES)
  if(WIN32)
    file(GLOB opencc_runtime_library
      "${opencc_package_directory}/bin/opencc.dll")
    install(FILES ${opencc_runtime_library}
      DESTINATION "${TIGER_BIN_INSTALL_PATH}" COMPONENT libs)
  else()
    find_library(opencc_runtime_library NAMES opencc
      PATHS "${opencc_package_directory}/lib"
      NO_DEFAULT_PATH NO_CACHE REQUIRED)
    install(CODE
      "file(INSTALL
        DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${TIGER_LIB_INSTALL_PATH}\"
        TYPE SHARED_LIBRARY
        FILES \"${opencc_runtime_library}\"
        FOLLOW_SYMLINK_CHAIN)"
      COMPONENT libs)
  endif()
endif()

# Preserve component license notices alongside the distributed GPL license.
install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/"
  DESTINATION "licenses"
  COMPONENT licenses)
