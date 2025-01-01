# ------------------------------------------------------------------------------
# Copyright (c) 2021
# SPDX-License-Identifier: MIT
#
# This file is part of the Tiger Template.
# Distributed under the MIT License. See LICENSE file for details.
#
# Created Date: 2021-4-12
# ------------------------------------------------------------------------------


file(GLOB proto_files "${CMAKE_CURRENT_LIST_DIR}/*.proto")

list(LENGTH proto_files proto_num)
if(${proto_num} LESS_EQUAL 0)
    message(FATAL_ERROR "Can not find proto file!")
    return()
endif()

set(proto_link proto)
set(proto_link proto PARENT_SCOPE)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${proto_files})

add_library(${proto_link} STATIC ${PROTO_SRCS} ${PROTO_HDRS})

# target_precompile_headers(${proto_link} 
#     PRIVATE
#         ${CMAKE_CURRENT_LIST_DIR}/pch/precomp_private.hpp
#     PUBLIC
#         ${CMAKE_CURRENT_LIST_DIR}/pch/precomp.hpp
# )

target_include_directories(${proto_link}
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
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