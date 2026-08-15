# SPDX-License-Identifier: GPL-3.0-or-later

file(REMOVE_RECURSE "${INSTALL_PREFIX}" "${TEST_HOME}")
file(MAKE_DIRECTORY
  "${INSTALL_PREFIX}" "${TEST_HOME}/config" "${TEST_HOME}/cache")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIRECTORY}"
    --prefix "${INSTALL_PREFIX}" --component libs
  RESULT_VARIABLE install_result)
if(NOT install_result EQUAL 0)
  message(FATAL_ERROR "Runtime install failed: ${install_result}")
endif()

set(installed_executable "${INSTALL_PREFIX}/bin/goldendict")
set(installed_wrapper "${INSTALL_PREFIX}/bin/goldendict.sh")
set(installed_library_directory "${INSTALL_PREFIX}/${INSTALL_LIBDIR}")
set(harfbuzz_subset
  "${installed_library_directory}/libharfbuzz-subset.so.0")
set(harfbuzz_main "${installed_library_directory}/libharfbuzz.so.0")
if(NOT EXISTS "${installed_executable}" OR NOT EXISTS "${installed_wrapper}")
  message(FATAL_ERROR "Runtime install omitted the GoldenDict executable or wrapper")
endif()
if(NOT EXISTS "${harfbuzz_subset}")
  message(FATAL_ERROR
    "Runtime install omitted Qt WebEngine dependency libharfbuzz-subset.so.0")
endif()
if(NOT EXISTS "${harfbuzz_main}")
  message(FATAL_ERROR
    "Runtime install omitted the ABI-matched HarfBuzz main library")
endif()
file(REAL_PATH "${harfbuzz_subset}" harfbuzz_subset_real)
if(NOT EXISTS "${harfbuzz_subset_real}" OR
   NOT harfbuzz_subset_real MATCHES "^${installed_library_directory}/")
  message(FATAL_ERROR
    "Installed HarfBuzz subset SONAME does not resolve inside the runtime bundle")
endif()
file(REAL_PATH "${harfbuzz_main}" harfbuzz_main_real)
if(NOT EXISTS "${harfbuzz_main_real}" OR
   NOT harfbuzz_main_real MATCHES "^${installed_library_directory}/")
  message(FATAL_ERROR
    "Installed HarfBuzz main SONAME does not resolve inside the runtime bundle")
endif()

file(GLOB qt_webengine_core
  "${installed_library_directory}/libQt6WebEngineCore.so.[0-9]")
list(LENGTH qt_webengine_core qt_webengine_core_count)
if(NOT qt_webengine_core_count EQUAL 1)
  message(FATAL_ERROR "Runtime install did not contain one Qt WebEngine Core SONAME")
endif()
list(GET qt_webengine_core 0 qt_webengine_core)

file(GLOB installed_project_libraries
  "${installed_library_directory}/libgoldendict_*.so.*")
foreach(elf_file IN ITEMS
    "${installed_executable}" "${qt_webengine_core}"
    ${installed_project_libraries})
  execute_process(
    COMMAND "${READELF_EXECUTABLE}" -d "${elf_file}"
    RESULT_VARIABLE readelf_result
    OUTPUT_VARIABLE dynamic_section)
  if(NOT readelf_result EQUAL 0)
    message(FATAL_ERROR "Could not inspect installed ELF file: ${elf_file}")
  endif()
  if(dynamic_section MATCHES "\\.conan2|${BUILD_DIRECTORY}")
    message(FATAL_ERROR
      "Installed runtime ELF retains a build or Conan cache RUNPATH: ${elf_file}")
  endif()
  if(elf_file STREQUAL qt_webengine_core)
    set(qt_webengine_dynamic_section "${dynamic_section}")
  endif()
endforeach()
if(NOT qt_webengine_dynamic_section MATCHES "libharfbuzz-subset\\.so\\.0")
  message(FATAL_ERROR
    "Qt WebEngine Core does not declare the expected HarfBuzz subset dependency")
endif()

execute_process(
  COMMAND /usr/bin/env -i
    "PATH=/usr/bin:/bin"
    "HOME=${TEST_HOME}"
    "QT_QPA_PLATFORM=offscreen"
    "QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu"
    "XDG_CONFIG_HOME=${TEST_HOME}/config"
    "XDG_CACHE_HOME=${TEST_HOME}/cache"
    "${installed_wrapper}" --smoke
  RESULT_VARIABLE smoke_result)
if(NOT smoke_result EQUAL 0)
  message(FATAL_ERROR
    "Installed GoldenDict clean-environment smoke failed: ${smoke_result}")
endif()
