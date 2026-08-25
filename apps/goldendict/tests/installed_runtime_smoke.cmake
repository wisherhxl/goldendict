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
set(installed_launcher
  "${INSTALL_PREFIX}/share/applications/org.goldendict.GoldenDict.desktop")
set(installed_icon
  "${INSTALL_PREFIX}/share/icons/hicolor/256x256/apps/goldendict.png")
set(installed_metainfo
  "${INSTALL_PREFIX}/share/metainfo/org.goldendict.GoldenDict.metainfo.xml")
set(installed_help_directory
  "${INSTALL_PREFIX}/share/goldendict/help")
set(installed_library_directory "${INSTALL_PREFIX}/${INSTALL_LIBDIR}")
set(harfbuzz_subset
  "${installed_library_directory}/libharfbuzz-subset.so.0")
set(harfbuzz_main "${installed_library_directory}/libharfbuzz.so.0")
if(NOT EXISTS "${installed_executable}" OR NOT EXISTS "${installed_wrapper}")
  message(FATAL_ERROR "Runtime install omitted the GoldenDict executable or wrapper")
endif()
if(NOT EXISTS "${installed_launcher}" OR NOT EXISTS "${installed_icon}" OR
   NOT EXISTS "${installed_metainfo}")
  message(FATAL_ERROR
    "Runtime install omitted the Linux launcher, icon, or metainfo")
endif()
file(READ "${installed_launcher}" launcher_contents)
foreach(required_entry IN ITEMS
    "Type=Application"
    "Terminal=false"
    "Icon=goldendict"
    "Exec=goldendict %u"
    "MimeType=x-scheme-handler/goldendict;x-scheme-handler/dict;")
  string(FIND "${launcher_contents}" "${required_entry}" entry_offset)
  if(entry_offset EQUAL -1)
    message(FATAL_ERROR
      "Installed Linux launcher omitted required entry: ${required_entry}")
  endif()
endforeach()
file(SHA256 "${installed_icon}" installed_icon_sha256)
if(NOT installed_icon_sha256 STREQUAL
    "935ccc5467aa896e6fc2061c059ef2b79fb791213fda76e508bb6b539af23bec")
  message(FATAL_ERROR "Installed Linux launcher icon differs from pinned legacy asset")
endif()
file(READ "${installed_metainfo}" metainfo_contents)
foreach(required_entry IN ITEMS
    "<id>org.goldendict.GoldenDict</id>"
    "<metadata_license>CC0-1.0</metadata_license>"
    "<project_license>GPL-3.0-or-later</project_license>"
    "<launchable type=\"desktop-id\">org.goldendict.GoldenDict.desktop</launchable>")
  string(FIND "${metainfo_contents}" "${required_entry}" entry_offset)
  if(entry_offset EQUAL -1)
    message(FATAL_ERROR
      "Installed Linux metainfo omitted required entry: ${required_entry}")
  endif()
endforeach()
foreach(help_file IN ITEMS gdhelp_en.qch gdhelp_ru.qch)
  if(NOT EXISTS "${installed_help_directory}/${help_file}")
    message(FATAL_ERROR
      "Runtime install omitted GoldenDict help collection: ${help_file}")
  endif()
endforeach()
file(SHA256 "${installed_help_directory}/gdhelp_en.qch"
  installed_english_help_sha256)
if(NOT installed_english_help_sha256 STREQUAL
    "abea4be4b9097aac81a3304c5ba1ed1dde5b31668a5efbbca9b11391897589ed")
  message(FATAL_ERROR
    "Installed English help collection differs from pinned legacy asset")
endif()
file(SHA256 "${installed_help_directory}/gdhelp_ru.qch"
  installed_russian_help_sha256)
if(NOT installed_russian_help_sha256 STREQUAL
    "ac38e0b62219d15ce4cbad52fe352f370167943cefb1eca29cb5c2e51a592f8c")
  message(FATAL_ERROR
    "Installed Russian help collection differs from pinned legacy asset")
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
    "XDG_SESSION_TYPE=x11"
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

set(installed_locale_directory
  "${INSTALL_PREFIX}/share/goldendict/locale")
foreach(catalog IN ITEMS ru_RU.qm qt_ru.qm qtbase_ru.qm)
  if(NOT EXISTS "${installed_locale_directory}/${catalog}")
    message(FATAL_ERROR "Missing installed translation catalog: ${catalog}")
  endif()
endforeach()

execute_process(
  COMMAND /usr/bin/env -i
    "PATH=/usr/bin:/bin"
    "HOME=${TEST_HOME}"
    "LANG=ru_RU.UTF-8"
    "XDG_SESSION_TYPE=x11"
    "QT_QPA_PLATFORM=offscreen"
    "XDG_CONFIG_HOME=${TEST_HOME}/translation-config"
    "XDG_CACHE_HOME=${TEST_HOME}/translation-cache"
    "${installed_wrapper}" --interface-language-russian-smoke
  RESULT_VARIABLE translation_smoke_result)
if(NOT translation_smoke_result EQUAL 0)
  message(FATAL_ERROR
    "Installed GoldenDict translation smoke failed: ${translation_smoke_result}")
endif()

execute_process(
  COMMAND /usr/bin/env -i
    "PATH=/usr/bin:/bin"
    "HOME=${TEST_HOME}"
    "XDG_SESSION_TYPE=x11"
    "QT_QPA_PLATFORM=offscreen"
    "XDG_CONFIG_HOME=${TEST_HOME}/config"
    "XDG_CACHE_HOME=${TEST_HOME}/cache"
    "${installed_wrapper}" --help-presentation-smoke
      "${installed_help_directory}"
  RESULT_VARIABLE help_smoke_result)
if(NOT help_smoke_result EQUAL 0)
  message(FATAL_ERROR
    "Installed GoldenDict help presentation smoke failed: ${help_smoke_result}")
endif()
