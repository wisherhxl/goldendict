# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict headless and desktop application core")

find_package(ZLIB REQUIRED)

ti_define_module(core
  REQUIRED PUBLIC MODULES tiger
  REQUIRED PRIVATE EXTRAS ZLIB::ZLIB
)

if(TARGET article_assembler_test)
  target_sources(article_assembler_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
  )
endif()

if(TARGET application_service_test)
  target_link_libraries(application_service_test PRIVATE ZLIB::ZLIB)
endif()

if(TARGET utf8_test)
  target_sources(utf8_test PRIVATE src/foundation/utf8.cc)
endif()

if(TARGET stardict_reader_test)
  target_sources(stardict_reader_test
    PRIVATE
      src/dictionary/generated_index.cc
      src/formats/stardict/stardict_reader.cc
  )
  target_link_libraries(stardict_reader_test PRIVATE ZLIB::ZLIB)
endif()

if(TARGET stardict_dictionary_test)
  target_sources(stardict_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/dictionary/generated_index.cc
      src/formats/stardict/stardict_dictionary.cc
      src/formats/stardict/stardict_reader.cc
      src/formats/stardict/stardict_resource.cc
  )
  target_link_libraries(stardict_dictionary_test PRIVATE ZLIB::ZLIB)
endif()

if(TARGET stardict_discovery_test)
  target_sources(stardict_discovery_test
    PRIVATE
      src/formats/stardict/stardict_discovery.cc
  )
endif()
