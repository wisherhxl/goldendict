# Creates a directory and checks if the creation was successful.
function(ti_create_directory directory_path)
  file(MAKE_DIRECTORY "${directory_path}")

  if(NOT EXISTS "${directory_path}")
    message(FATAL_ERROR "Failed to create directory: ${directory_path}")
  else()
    message(STATUS "Directory created successfully: ${directory_path}")
  endif()
endfunction()

# Get the current year and month.
function(ti_get_date_components year month)
  string(TIMESTAMP DATE_TIMESTAMP "%Y;%m" UTC)
  string(REGEX REPLACE "([0-9]+)\n([0-9]+)" "\\1;\\2" DATE_OUTPUT
                       "${DATE_TIMESTAMP}")

  list(GET DATE_OUTPUT 0 DATE_YEAR)
  list(GET DATE_OUTPUT 1 DATE_MONTH)
  set(${year}
      ${DATE_YEAR}
      PARENT_SCOPE)
  set(${month}
      ${DATE_MONTH}
      PARENT_SCOPE)
endfunction()

macro(ti_glob_protos protos_dir)

  set(TIGER_HAVE_PROTOS OFF)
  file(
    GLOB_RECURSE proto_files
    LIST_DIRECTORIES false
    "${protos_dir}/*.proto")

  list(LENGTH proto_files PROTO_FILES_LENGTH)
  if(${PROTO_FILES_LENGTH} LESS 1)
    message(STATUS "No proto files has been found: ${protos_dir}")
  else()
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${proto_files})

    set(proto_link protos)
    set(TIGER_HAVE_PROTOS ON)

    add_library(${proto_link} STATIC ${PROTO_SRCS} ${PROTO_HDRS})

    target_include_directories(${proto_link} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

    target_link_libraries(${proto_link} PUBLIC ${protobuf_link})

    set_target_properties(
      ${proto_link}
      PROPERTIES DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
                 ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
                 RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
                 OUTPUT_NAME "${proto_link}")
    set_property(TARGET ${proto_link} PROPERTY FOLDER "extra")
  endif()
endmacro()

# Find modules and
macro(ti_assemble_modules)
  add_definitions(-D__TIGER_BUILD=1)

  if(NOT TIGER_MODULES_PATH)
    set(TIGER_MODULES_PATH "${${PROJECT_NAME}_SOURCE_DIR}/modules")
  endif()

  ti_glob_modules(${TIGER_MODULES_PATH} ${TIGER_EXTRA_MODULES_PATH})

  # build lists of modules to be documented
  set(TIGER_MODULES_MAIN "")
  set(TIGER_MODULES_EXTRA "")

  foreach(mod ${TIGER_MODULES_BUILD} ${TIGER_MODULES_DISABLED_USER}
              ${TIGER_MODULES_DISABLED_AUTO} ${TIGER_MODULES_DISABLED_FORCE})
    string(REGEX REPLACE "^${TI_INTERNAL_NAME}_" "" mod "${mod}")
    if("${TIGER_MODULE_${TI_INTERNAL_NAME}_${mod}_LOCATION}" STREQUAL
       "${${PROJECT_NAME}_SOURCE_DIR}/modules/${mod}")
      list(APPEND TIGER_MODULES_MAIN ${mod})
    else()
      list(APPEND TIGER_MODULES_EXTRA ${mod})
    endif()
  endforeach()
  ti_list_sort(TIGER_MODULES_MAIN)
  ti_list_sort(TIGER_MODULES_EXTRA)

  if(NOT TI_FIXED_ORDER_MODULES)
    set(TI_FIXED_ORDER_MODULES base)
  endif()
  set(FIXED_ORDER_MODULES ${TI_FIXED_ORDER_MODULES})
  list(REMOVE_ITEM TIGER_MODULES_MAIN ${FIXED_ORDER_MODULES})
  set(TIGER_MODULES_MAIN ${FIXED_ORDER_MODULES} ${TIGER_MODULES_MAIN})

  set(TIGER_MODULES_MAIN
      ${TIGER_MODULES_MAIN}
      CACHE INTERNAL "List of main modules" FORCE)
  set(TIGER_MODULES_EXTRA
      ${TIGER_MODULES_EXTRA}
      CACHE INTERNAL "List of extra modules" FORCE)
endmacro()

macro(ti_assemble_applications)
  if(NOT TIGER_APPLICATIONS_PATH)
    set(TIGER_APPLICATIONS_PATH "${${PROJECT_NAME}_SOURCE_DIR}/apps")
  endif()
  ti_glob_applications(${TIGER_APPLICATIONS_PATH})
endmacro()

macro(ti_assemble_protos)
  if(NOT TIGER_PROTOS_PATH)
    set(TIGER_PROTOS_PATH "${${PROJECT_NAME}_SOURCE_DIR}/protos")
  endif()
  ti_glob_protos(${TIGER_PROTOS_PATH})
endmacro()
