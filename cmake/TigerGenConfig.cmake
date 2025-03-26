# --------------------------------------------------------------------------------------------
#  Installation for CMake Module:  TigerConfig.cmake
#  Part 1/3: ${BIN_DIR}/TigerConfig.cmake              -> For use *without* "make install"
#  Part 2/3: ${BIN_DIR}/unix-install/TigerConfig.cmake -> For use with "make install"
#  Part 3/3: ${BIN_DIR}/win-install/TigerConfig.cmake  -> For use within binary installers/packages
# -------------------------------------------------------------------------------------------

if(INSTALL_TO_MANGLED_PATHS)
  set(${TI_PROJECT_NAME}_USE_MANGLED_PATHS_CONFIGCMAKE TRUE)
else()
  set(${TI_PROJECT_NAME}_USE_MANGLED_PATHS_CONFIGCMAKE FALSE)
endif()

if(HAVE_CUDA)
  if(ENABLE_CUDA_FIRST_CLASS_LANGUAGE)
    ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-CUDALanguage.cmake.in" CUDA_CONFIGCMAKE @ONLY)
  else()
    ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-CUDA.cmake.in" CUDA_CONFIGCMAKE @ONLY)
  endif()
endif()

if(ANDROID)
  if(NOT ANDROID_NATIVE_API_LEVEL)
    set(${TI_PROJECT_NAME}_ANDROID_NATIVE_API_LEVEL_CONFIGCMAKE 0)
  else()
    set(${TI_PROJECT_NAME}_ANDROID_NATIVE_API_LEVEL_CONFIGCMAKE "${ANDROID_NATIVE_API_LEVEL}")
  endif()
  ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-ANDROID.cmake.in" ANDROID_CONFIGCMAKE @ONLY)
endif()

set(TIGER_MODULES_CONFIGCMAKE ${TIGER_MODULES_PUBLIC})

if(BUILD_FAT_JAVA_LIB AND HAVE_tiger_java)
  list(APPEND TIGER_MODULES_CONFIGCMAKE tiger_java)
endif()

if(BUILD_OBJC AND HAVE_tiger_objc)
  list(APPEND TIGER_MODULES_CONFIGCMAKE tiger_objc)
endif()


# -------------------------------------------------------------------------------------------
#  Part 1/3: ${BIN_DIR}/TigerConfig.cmake              -> For use *without* "make install"
# -------------------------------------------------------------------------------------------
set(${TI_PROJECT_NAME}_INCLUDE_DIRS_CONFIGCMAKE "\"${TIGER_CONFIG_FILE_INCLUDE_DIR}\" \"${${PROJECT_NAME}_SOURCE_DIR}/include\"")

foreach(m ${TIGER_MODULES_BUILD})
  if(EXISTS "${TIGER_MODULE_${m}_LOCATION}/include")
    set(${PROJECT_NAME}_INCLUDE_DIRS_CONFIGCMAKE "${${PROJECT_NAME}_INCLUDE_DIRS_CONFIGCMAKE} \"${TIGER_MODULE_${m}_LOCATION}/include\"")
  endif()
endforeach()

export(EXPORT TigerModules FILE "${CMAKE_BINARY_DIR}/TigerModules.cmake")

if(TARGET ippicv AND NOT BUILD_SHARED_LIBS)
  set(USE_IPPICV TRUE)
  file(RELATIVE_PATH IPPICV_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_BINARY_DIR}" "${IPPICV_LOCATION_PATH}")
  ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-IPPICV.cmake.in" IPPICV_CONFIGCMAKE @ONLY)
else()
  set(USE_IPPICV FALSE)
endif()

if(TARGET ippiw AND NOT BUILD_SHARED_LIBS AND IPPIW_INSTALL_PATH)
  set(USE_IPPIW TRUE)
  file(RELATIVE_PATH IPPIW_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_BINARY_DIR}" "${IPPIW_LOCATION_PATH}")
  ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-IPPIW.cmake.in" IPPIW_CONFIGCMAKE @ONLY)
else()
  set(USE_IPPIW FALSE)
endif()

ti_cmake_hook(PRE_CMAKE_CONFIG_BUILD)
configure_file("${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/TigerConfig.cmake.in" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake" @ONLY)
#support for version checking when finding tiger. find_package(${PROJECT_NAME} 2.3.1 EXACT) should now work.
configure_file("${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/TigerConfig-version.cmake.in" "${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config-version.cmake" @ONLY)

# --------------------------------------------------------------------------------------------
#  Part 2/3: ${BIN_DIR}/unix-install/TigerConfig.cmake -> For use *with* "make install"
# -------------------------------------------------------------------------------------------
file(RELATIVE_PATH ${PROJECT_NAME}_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_INSTALL_PREFIX}/${TIGER_CONFIG_INSTALL_PATH}/" ${CMAKE_INSTALL_PREFIX})
if (IS_ABSOLUTE ${TIGER_INCLUDE_INSTALL_PATH})
  set(${PROJECT_NAME}_INCLUDE_DIRS_CONFIGCMAKE "\"${TIGER_INCLUDE_INSTALL_PATH}\"")
else()
  set(${PROJECT_NAME}_INCLUDE_DIRS_CONFIGCMAKE "\"\${${PROJECT_NAME}_INSTALL_PATH}/${TIGER_INCLUDE_INSTALL_PATH}\"")
endif()

if(USE_IPPICV)
  file(RELATIVE_PATH IPPICV_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_INSTALL_PREFIX}" "${IPPICV_INSTALL_PATH}")
  ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-IPPICV.cmake.in" IPPICV_CONFIGCMAKE @ONLY)
endif()
if(USE_IPPIW)
  file(RELATIVE_PATH IPPIW_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_INSTALL_PREFIX}" "${IPPIW_INSTALL_PATH}")
  ti_cmake_configure("${CMAKE_CURRENT_LIST_DIR}/templates/TigerConfig-IPPIW.cmake.in" IPPIW_CONFIGCMAKE @ONLY)
endif()

function(ti_gen_config TMP_DIR NESTED_PATH ROOT_NAME)
  ti_path_join(__install_nested "${TIGER_CONFIG_INSTALL_PATH}" "${NESTED_PATH}")
  ti_path_join(__tmp_nested "${TMP_DIR}" "${NESTED_PATH}")

  file(RELATIVE_PATH ${PROJECT_NAME}_INSTALL_PATH_RELATIVE_CONFIGCMAKE "${CMAKE_INSTALL_PREFIX}/${__install_nested}" "${CMAKE_INSTALL_PREFIX}/")

  ti_cmake_hook(PRE_CMAKE_CONFIG_INSTALL)
  configure_file("${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/TigerConfig-version.cmake.in" "${TMP_DIR}/${PROJECT_NAME}Config-version.cmake" @ONLY)

  configure_file("${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/TigerConfig.cmake.in" "${__tmp_nested}/${PROJECT_NAME}Config.cmake" @ONLY)
  install(EXPORT TigerModules DESTINATION "${__install_nested}" FILE ${PROJECT_NAME}Modules.cmake COMPONENT dev)
  install(FILES
      "${TMP_DIR}/${PROJECT_NAME}Config-version.cmake"
      "${__tmp_nested}/${PROJECT_NAME}Config.cmake"
      DESTINATION "${__install_nested}" COMPONENT dev)

  if(ROOT_NAME)
    # message(FATAL_ERROR "ROOT_NAME: ${ROOT_NAME}")
    # Root config file
    configure_file("${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/${ROOT_NAME}" "${TMP_DIR}/${PROJECT_NAME}Config.cmake" @ONLY)
    install(FILES
        "${TMP_DIR}/${PROJECT_NAME}Config-version.cmake"
        "${TMP_DIR}/${PROJECT_NAME}Config.cmake"
        DESTINATION "${TIGER_CONFIG_INSTALL_PATH}" COMPONENT dev)
  endif()
endfunction()

if((CMAKE_HOST_SYSTEM_NAME MATCHES "Linux" OR UNIX) AND NOT ANDROID)
  ti_gen_config("${CMAKE_BINARY_DIR}/unix-install" "" "")
endif()

if(ANDROID)
  ti_gen_config("${CMAKE_BINARY_DIR}/unix-install" "abi-${ANDROID_NDK_ABI_NAME}" "TigerConfig.root-ANDROID.cmake.in")
endif()

# --------------------------------------------------------------------------------------------
#  Part 3/3: ${BIN_DIR}/win-install/TigerConfig.cmake  -> For use within binary installers/packages
# --------------------------------------------------------------------------------------------
if(WIN32)
  if(CMAKE_HOST_SYSTEM_NAME MATCHES Windows AND NOT TIGER_SKIP_CMAKE_ROOT_CONFIG)
    ti_gen_config("${CMAKE_BINARY_DIR}/win-install"
                   "${TIGER_INSTALL_BINARIES_PREFIX}${TIGER_INSTALL_BINARIES_SUFFIX}"
                   "TigerConfig.root-WIN32.cmake.in")
  else()
    ti_gen_config("${CMAKE_BINARY_DIR}/win-install" "" "")
  endif()
endif()