# If protobuf is found - libprotobuf target is available

set(HAVE_PROTOBUF FALSE)

# BUILD_PROTOBUF=OFF: Custom manual protobuf configuration (see find_package(Protobuf) for details):
# - Protobuf_INCLUDE_DIR
# - Protobuf_LIBRARY
# - Protobuf_PROTOC_EXECUTABLE

function(get_protobuf_version version include)
  file(STRINGS "${include}/google/protobuf/stubs/common.h" ver REGEX "#define GOOGLE_PROTOBUF_VERSION [0-9]+")
  string(REGEX MATCHALL "[0-9]+" ver ${ver})
  math(EXPR major "${ver} / 1000000")
  math(EXPR minor "${ver} / 1000 % 1000")
  math(EXPR patch "${ver} % 1000")
  set(${version} "${major}.${minor}.${patch}" PARENT_SCOPE)
endfunction()

unset(Protobuf_VERSION CACHE)
find_package(Protobuf CONFIG QUIET)

# Backwards compatibility
# Define camel case versions of input variables
foreach(UPPER
  PROTOBUF_FOUND
  PROTOBUF_LIBRARY
  PROTOBUF_INCLUDE_DIR
  PROTOBUF_VERSION
  )
  if (DEFINED ${UPPER})
      string(REPLACE "PROTOBUF_" "Protobuf_" Camel ${UPPER})
      if (NOT DEFINED ${Camel})
          set(${Camel} ${${UPPER}})
      endif()
  endif()
endforeach()
# end of compatibility block

if(Protobuf_FOUND)
  if(TARGET protobuf::libprotobuf)
    set(Protobuf_LIBRARIES "protobuf::libprotobuf")
  else()
    add_library(libprotobuf UNKNOWN IMPORTED)
    set_target_properties(libprotobuf PROPERTIES
      IMPORTED_LOCATION "${Protobuf_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}"
      INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIR}"
    )
    get_protobuf_version(Protobuf_VERSION "${Protobuf_INCLUDE_DIR}")
    set(Protobuf_LIBRARIES "libprotobuf")
  endif()
  set(HAVE_PROTOBUF TRUE)
  ti_list_components(Protobuf)
endif()

if(HAVE_PROTOBUF AND PROTOBUF_UPDATE_FILES AND NOT COMMAND PROTOBUF_GENERATE_CPP)
  message(FATAL_ERROR "Can't configure protobuf dependency (BUILD_PROTOBUF=${BUILD_PROTOBUF} PROTOBUF_UPDATE_FILES=${PROTOBUF_UPDATE_FILES})")
endif()

if(HAVE_PROTOBUF)
  if(BUILD_PROTOBUF)
    ti_add_thirdparty_status(protobuf
      DISPLAY "Protobuf"
      VERSION "${Protobuf_VERSION}"
      LOCATION "build")
  elseif(Protobuf_LIBRARY)
    ti_add_thirdparty_status(protobuf
      DISPLAY "Protobuf"
      VERSION "${Protobuf_VERSION}"
      LOCATION "${Protobuf_LIBRARY}"
      TARGETS ${Protobuf_LIBRARIES}
      PACKAGE Protobuf
      CONFIG)
  else()
    ti_add_thirdparty_status(protobuf
      DISPLAY "Protobuf"
      VERSION "${Protobuf_VERSION}"
      TARGETS ${Protobuf_LIBRARIES}
      PACKAGE Protobuf
      CONFIG)
  endif()
endif()
