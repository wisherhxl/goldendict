# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict headless and desktop application core")

find_package(BZip2 REQUIRED)
find_package(ICU REQUIRED CONFIG COMPONENTS uc)
find_package(ZLIB REQUIRED)

ti_define_module(core
  REQUIRED PUBLIC MODULES tiger
  REQUIRED PRIVATE EXTRAS BZip2::BZip2 ICU::uc Qt6::Core ZLIB::ZLIB
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
  target_link_libraries(application_service_test
    PRIVATE BZip2::BZip2 ZLIB::ZLIB
  )
endif()

if(TARGET utf8_test)
  target_sources(utf8_test PRIVATE src/foundation/utf8.cc)
endif()

if(TARGET text_folding_test)
  target_sources(text_folding_test
    PRIVATE
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
  )
  target_link_libraries(text_folding_test PRIVATE ICU::uc)
endif()

if(TARGET text_encoding_test)
  target_sources(text_encoding_test PRIVATE src/foundation/text_encoding.cc)
  target_link_libraries(text_encoding_test PRIVATE ICU::uc)
endif()

if(TARGET stardict_reader_test)
  target_sources(stardict_reader_test
    PRIVATE
      src/dictionary/generated_index.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/stardict/stardict_reader.cc
  )
  target_link_libraries(stardict_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET stardict_dictionary_test)
  target_sources(stardict_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/dictionary/generated_index.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/stardict/stardict_dictionary.cc
      src/formats/stardict/stardict_reader.cc
      src/formats/stardict/stardict_resource.cc
  )
  target_link_libraries(stardict_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET stardict_discovery_test)
  target_sources(stardict_discovery_test
    PRIVATE
      src/formats/stardict/stardict_discovery.cc
  )
endif()

if(TARGET dictd_discovery_test)
  target_sources(dictd_discovery_test
    PRIVATE
      src/formats/dictd/dictd_discovery.cc
  )
endif()

if(TARGET dictd_reader_test)
  target_sources(dictd_reader_test
    PRIVATE
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/dictd/dictd_reader.cc
  )
  target_link_libraries(dictd_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET dictd_dictionary_test)
  target_sources(dictd_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/dictd/dictd_dictionary.cc
      src/formats/dictd/dictd_reader.cc
  )
  target_link_libraries(dictd_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET sdict_discovery_test)
  target_sources(sdict_discovery_test
    PRIVATE
      src/formats/sdict/sdict_discovery.cc
  )
endif()

if(TARGET sdict_reader_test)
  target_sources(sdict_reader_test
    PRIVATE
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/sdict/sdict_reader.cc
  )
  target_link_libraries(sdict_reader_test
    PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET sdict_dictionary_test)
  target_sources(sdict_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/sdict/sdict_dictionary.cc
      src/formats/sdict/sdict_reader.cc
  )
  target_link_libraries(sdict_dictionary_test
    PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET xdxf_discovery_test)
  target_sources(xdxf_discovery_test
    PRIVATE
      src/formats/xdxf/xdxf_discovery.cc
  )
endif()

if(TARGET xdxf_reader_test)
  target_sources(xdxf_reader_test
    PRIVATE
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/xdxf/xdxf_reader.cc
  )
  target_link_libraries(xdxf_reader_test
    PRIVATE ICU::uc Qt6::Core ZLIB::ZLIB
  )
endif()

if(TARGET xdxf_dictionary_test)
  target_sources(xdxf_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/xdxf/xdxf_dictionary.cc
      src/formats/xdxf/xdxf_reader.cc
  )
  target_link_libraries(xdxf_dictionary_test
    PRIVATE ICU::uc Qt6::Core ZLIB::ZLIB
  )
endif()
