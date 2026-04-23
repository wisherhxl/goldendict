# ----------------------------------------------------------------------------
# Copyright (c) 2021 SPDX-License-Identifier: MIT
#
# This file is the Root CMake file for Tiger Template. Distributed under the MIT
# License. See LICENSE file for details.
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
set(TI_PROJECT_NAME "")
# Namespace of the project.
# Leave empty to use TI_PROJECT_NAME automatically.
set(TI_PROJECT_NAMESPACE "")
# Description of the project
set(TI_PROJECT_INFO "A C++ Template for CMake and Conan")

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
set(TI_BASE_MODULE "")
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
