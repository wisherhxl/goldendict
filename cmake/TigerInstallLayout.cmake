# message(STATUS "Initial install layout:")
# ti_cmake_dump_vars("TIGER_.*_INSTALL_PATH")

if(ANDROID)

  ti_update(TIGER_BIN_INSTALL_PATH            "sdk/native/bin/${ANDROID_NDK_ABI_NAME}")
  ti_update(TIGER_TEST_INSTALL_PATH           "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_SAMPLES_BIN_INSTALL_PATH    "sdk/native/samples/${ANDROID_NDK_ABI_NAME}")
  ti_update(TIGER_LIB_INSTALL_PATH            "sdk/native/libs/${ANDROID_NDK_ABI_NAME}")
  ti_update(TIGER_LIB_ARCHIVE_INSTALL_PATH    "sdk/native/staticlibs/${ANDROID_NDK_ABI_NAME}")
  ti_update(TIGER_3P_LIB_INSTALL_PATH         "sdk/native/3rdparty/libs/${ANDROID_NDK_ABI_NAME}")
  ti_update(TIGER_CONFIG_INSTALL_PATH         "sdk/native/jni")
  ti_update(TIGER_INCLUDE_INSTALL_PATH        "sdk/native/jni/include")
  ti_update(TIGER_OTHER_INSTALL_PATH          "sdk/etc")
  ti_update(TIGER_SAMPLES_SRC_INSTALL_PATH    "samples/native")
  ti_update(TIGER_LICENSES_INSTALL_PATH       "${TIGER_OTHER_INSTALL_PATH}/licenses")
  ti_update(TIGER_TEST_DATA_INSTALL_PATH      "${TIGER_OTHER_INSTALL_PATH}/testdata")
  ti_update(TIGER_DOC_INSTALL_PATH            "doc")
  ti_update(TIGER_JAR_INSTALL_PATH            ".")
  ti_update(TIGER_JNI_INSTALL_PATH            "${TIGER_LIB_INSTALL_PATH}")
  ti_update(TIGER_JNI_BIN_INSTALL_PATH        "${TIGER_JNI_INSTALL_PATH}")

elseif(WIN32 AND CMAKE_HOST_SYSTEM_NAME MATCHES Windows)

  if(DEFINED ${PROJECT_NAME}_RUNTIME AND DEFINED ${PROJECT_NAME}_ARCH)
    ti_update(TIGER_INSTALL_BINARIES_PREFIX "${${PROJECT_NAME}_ARCH}/${${PROJECT_NAME}_RUNTIME}/")
  else()
    message(STATUS "Can't detect runtime and/or arch")
    ti_update(TIGER_INSTALL_BINARIES_PREFIX "")
  endif()
  if(${PROJECT_NAME}_STATIC)
    ti_update(TIGER_INSTALL_BINARIES_SUFFIX "staticlib")
  else()
    ti_update(TIGER_INSTALL_BINARIES_SUFFIX "lib")
  endif()
  if(INSTALL_CREATE_DISTRIB)
    set(_jni_suffix "/${${PROJECT_NAME}_ARCH}")
  else()
    set(_jni_suffix "")
  endif()

  ti_update(TIGER_BIN_INSTALL_PATH           "${TIGER_INSTALL_BINARIES_PREFIX}bin")
  ti_update(TIGER_TEST_INSTALL_PATH          "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_SAMPLES_BIN_INSTALL_PATH   "${TIGER_INSTALL_BINARIES_PREFIX}samples")
  ti_update(TIGER_LIB_INSTALL_PATH           "${TIGER_INSTALL_BINARIES_PREFIX}${TIGER_INSTALL_BINARIES_SUFFIX}")
  ti_update(TIGER_LIB_ARCHIVE_INSTALL_PATH   "${TIGER_LIB_INSTALL_PATH}")
  ti_update(TIGER_3P_LIB_INSTALL_PATH        "${TIGER_INSTALL_BINARIES_PREFIX}staticlib")
  ti_update(TIGER_CONFIG_INSTALL_PATH        ".")
  ti_update(TIGER_INCLUDE_INSTALL_PATH       "include")
  ti_update(TIGER_OTHER_INSTALL_PATH         "etc")
  ti_update(TIGER_SAMPLES_SRC_INSTALL_PATH   "samples")
  ti_update(TIGER_LICENSES_INSTALL_PATH      "${TIGER_OTHER_INSTALL_PATH}/licenses")
  ti_update(TIGER_TEST_DATA_INSTALL_PATH     "testdata")
  ti_update(TIGER_DOC_INSTALL_PATH           "doc")
  ti_update(TIGER_JAR_INSTALL_PATH           "java")
  ti_update(TIGER_JNI_INSTALL_PATH           "java${_jni_suffix}")
  ti_update(TIGER_JNI_BIN_INSTALL_PATH       "${TIGER_JNI_INSTALL_PATH}")

elseif(QNX)
  ti_update(TIGER_BIN_INSTALL_PATH         "${CPUVARDIR}/usr/bin")
  ti_update(TIGER_TEST_INSTALL_PATH        "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_SAMPLES_BIN_INSTALL_PATH "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_LIB_INSTALL_PATH         "${CPUVARDIR}/usr/lib")
  ti_update(TIGER_LIB_ARCHIVE_INSTALL_PATH "${TIGER_LIB_INSTALL_PATH}")
  ti_update(TIGER_3P_LIB_INSTALL_PATH      "${CPUVARDIR}/usr/lib")
  ti_update(TIGER_CONFIG_INSTALL_PATH      "${CPUVARDIR}/usr/share/Tiger")
  ti_update(TIGER_INCLUDE_INSTALL_PATH     "usr/include/Tiger/tiger4")
  ti_update(TIGER_OTHER_INSTALL_PATH       "usr/share/Tiger")
  ti_update(TIGER_SAMPLES_SRC_INSTALL_PATH "samples/native")
  ti_update(TIGER_LICENSES_INSTALL_PATH    "${TIGER_OTHER_INSTALL_PATH}/licenses")
  ti_update(TIGER_TEST_DATA_INSTALL_PATH   "${TIGER_OTHER_INSTALL_PATH}/testdata")
  ti_update(TIGER_DOC_INSTALL_PATH         "doc")
  ti_update(TIGER_JAR_INSTALL_PATH         "${CMAKE_INSTALL_DATAROOTDIR}/java/tiger4")
  ti_update(TIGER_JNI_INSTALL_PATH         "${TIGER_JAR_INSTALL_PATH}")
  ti_update(TIGER_JNI_BIN_INSTALL_PATH     "${TIGER_JNI_INSTALL_PATH}")

else() # UNIX

  include(GNUInstallDirs)
  ti_update(TIGER_BIN_INSTALL_PATH           "bin")
  ti_update(TIGER_TEST_INSTALL_PATH          "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_SAMPLES_BIN_INSTALL_PATH   "${TIGER_BIN_INSTALL_PATH}")
  ti_update(TIGER_LIB_INSTALL_PATH           "${CMAKE_INSTALL_LIBDIR}")
  ti_update(TIGER_LIB_ARCHIVE_INSTALL_PATH   "${TIGER_LIB_INSTALL_PATH}")
  ti_update(TIGER_3P_LIB_INSTALL_PATH        "${TIGER_LIB_INSTALL_PATH}/tiger5/3rdparty")
  ti_update(TIGER_CONFIG_INSTALL_PATH        "${TIGER_LIB_INSTALL_PATH}/cmake/tiger5")
  ti_update(TIGER_INCLUDE_INSTALL_PATH       "${CMAKE_INSTALL_INCLUDEDIR}/tiger5")
  ti_update(TIGER_OTHER_INSTALL_PATH         "${CMAKE_INSTALL_DATAROOTDIR}/tiger5")
  ti_update(TIGER_SAMPLES_SRC_INSTALL_PATH   "${TIGER_OTHER_INSTALL_PATH}/samples")
  ti_update(TIGER_LICENSES_INSTALL_PATH      "${CMAKE_INSTALL_DATAROOTDIR}/licenses/tiger5")
  ti_update(TIGER_TEST_DATA_INSTALL_PATH     "${TIGER_OTHER_INSTALL_PATH}/testdata")
  ti_update(TIGER_DOC_INSTALL_PATH           "${CMAKE_INSTALL_DATAROOTDIR}/doc/tiger5")
  ti_update(TIGER_JAR_INSTALL_PATH           "${CMAKE_INSTALL_DATAROOTDIR}/java/tiger5")
  ti_update(TIGER_JNI_INSTALL_PATH           "${TIGER_JAR_INSTALL_PATH}")
  ti_update(TIGER_JNI_BIN_INSTALL_PATH       "${TIGER_JNI_INSTALL_PATH}")

endif()

ti_update(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${TIGER_LIB_INSTALL_PATH}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

if(INSTALL_TO_MANGLED_PATHS)
  foreach(v
      TIGER_INCLUDE_INSTALL_PATH
      # file names include version (.so/.dll): TIGER_LIB_INSTALL_PATH
      TIGER_CONFIG_INSTALL_PATH
      TIGER_3P_LIB_INSTALL_PATH
      TIGER_SAMPLES_SRC_INSTALL_PATH
      TIGER_DOC_INSTALL_PATH
      # JAR file name includes version: TIGER_JAR_INSTALL_PATH
      TIGER_TEST_DATA_INSTALL_PATH
      TIGER_OTHER_INSTALL_PATH
    )
    string(REGEX REPLACE "${TI_INTERNAL_NAME}[0-9]*" "${TI_INTERNAL_NAME}-${TIGER_VERSION}" ${v} "${${v}}")
  endforeach()
endif()

# message(STATUS "Final install layout:")
# ti_cmake_dump_vars("TIGER_.*_INSTALL_PATH")
