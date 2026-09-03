# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict controlled external program integrations")
set(TIGER_MODULE_TYPE STATIC)

ti_define_module(external
  INTERNAL
  REQUIRED PRIVATE EXTRAS Qt6::Core
)

if(TARGET external_program_source_test)
  ti_add_executable(goldendict_external_program_test_helper
    test_support/external_program_test_helper.cpp
  )
  set_target_properties(goldendict_external_program_test_helper PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}"
  )
  target_sources(external_program_source_test PRIVATE
    src/external_program_source.cc
  )
  target_link_libraries(external_program_source_test PRIVATE Qt6::Core)
  target_compile_definitions(external_program_source_test PRIVATE
    GOLDENDICT_EXTERNAL_PROGRAM_TEST_HELPER="$<TARGET_FILE:goldendict_external_program_test_helper>"
  )
  add_dependencies(external_program_source_test
    goldendict_external_program_test_helper
  )
endif()
