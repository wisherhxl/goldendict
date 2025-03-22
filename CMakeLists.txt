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

# ============ Project Name ============
set(TI_ORG_NAME "") # Name of organization
set(TI_PROJECT_CONTACT "") # Email of project maintainer
set(TI_PROJECT_NAME "Rago") # Name of the project
# Description of the project
set(TI_PROJECT_INFO "A C++ Template for CMake and Conan")

# ============ Project Version ============
set(TI_VERSION_MAJOR 0) # Major version
set(TI_VERSION_MINOR 1) # Minor version
set(TI_VERSION_REVISION 0) # Revision version

# ============ Project Definition ============
project(${TI_PROJECT_NAME} CXX C)
enable_language(CXX C)

# ============ Modules Information ============
# TI_FIXED_ORDER_MODULES: Modules listed here are built first, in the order
# specified. Remaining modules are built in a random order.
set(TI_FIXED_ORDER_MODULES base)

# ============ Applications Information ============

# ============ Finalize Settings ============
include(cmake/TigerMainSettings.cmake)
include(cmake/TigerMainFinalize.cmake)
