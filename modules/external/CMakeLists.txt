# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict controlled external program integrations")

ti_define_module(external
  INTERNAL
  REQUIRED PRIVATE EXTRAS Qt6::Core
)

if(TARGET external_program_source_test)
  target_sources(external_program_source_test PRIVATE
    src/external_program_source.cc
  )
  target_link_libraries(external_program_source_test PRIVATE Qt6::Core)
endif()
