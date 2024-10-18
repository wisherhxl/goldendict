file(GLOB proto_files "${CMAKE_CURRENT_LIST_DIR}/*.proto")

list(LENGTH proto_files proto_num)
if(${proto_num} LESS_EQUAL 0)
    message(FATAL_ERROR "Can not find proto file!")
    return()
endif()

set(proto_link proto)
set(proto_link proto PARENT_SCOPE)

add_library(${proto_link} SHARED)

protobuf_generate(
    LANGUAGE cpp
    TARGET ${proto_link}
    EXPORT_MACRO PROTO_EXPORTS
    PROTOS ${proto_files})

target_precompile_headers(${proto_link} 
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/pch/precomp_private.hpp
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/pch/precomp.hpp
)

target_link_libraries(${proto_link}
    PUBLIC
        ${protobuf_link}
)

set_target_properties(${proto_link}
      PROPERTIES
        DEBUG_POSTFIX "${TIGER_DEBUG_POSTFIX}"
        ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
        RUNTIME_OUTPUT_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}
        OUTPUT_NAME "${proto_link}"
  )
set_property(TARGET ${proto_link} PROPERTY FOLDER "extra")