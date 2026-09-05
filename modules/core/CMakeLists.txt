# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict headless and desktop application core")

find_package(BZip2 REQUIRED)
find_package(expat REQUIRED CONFIG)
find_package(ICU REQUIRED CONFIG COMPONENTS uc)
find_package(hunspell REQUIRED CONFIG)
find_package(OpenCC REQUIRED CONFIG GLOBAL)
set(opencc_package_directories "")
foreach(opencc_configuration IN ITEMS
    DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  if(DEFINED opencc_PACKAGE_FOLDER_${opencc_configuration})
    list(APPEND opencc_package_directories
      "${opencc_PACKAGE_FOLDER_${opencc_configuration}}")
  endif()
endforeach()
list(REMOVE_DUPLICATES opencc_package_directories)
list(LENGTH opencc_package_directories opencc_package_directory_count)
if(NOT opencc_package_directory_count EQUAL 1)
  message(FATAL_ERROR
    "OpenCC must publish one resolved Conan package directory")
endif()
set_property(GLOBAL PROPERTY GOLDENDICT_OPENCC_PACKAGE_DIRECTORY
  "${opencc_package_directories}")
find_package(PCRE2 REQUIRED CONFIG)
find_package(Vorbis REQUIRED CONFIG)
find_package(ZLIB REQUIRED)

ti_define_module(core
  REQUIRED PUBLIC MODULES tiger
  REQUIRED PRIVATE EXTRAS BZip2::BZip2 expat::expat hunspell::hunspell ICU::uc OpenCC::OpenCC PCRE2::8BIT Vorbis::vorbisfile ZLIB::ZLIB
)

if(BUILD_TESTS)
  ti_add_executable(qt6_real_dictionary_observer
    tools/qt6_real_dictionary_observer.cpp)
  ti_target_include_modules_recurse(qt6_real_dictionary_observer
    goldendict_core)
  ti_target_link_libraries(qt6_real_dictionary_observer
    goldendict_core ${QT_NAMESPACE}::Core)
  add_test(NAME qt6_real_dictionary_observer_duplicate_option_rejection
    COMMAND qt6_real_dictionary_observer
      --dictionary-root unused-corpus
      --index-root unused-index
      --interface-language en_US
      --conditions-sha256 unused-hash
      --scenario clean-discovery
      --output unused-output
      --output escaped-output)
  set_tests_properties(qt6_real_dictionary_observer_duplicate_option_rejection
    PROPERTIES WILL_FAIL TRUE)
  add_test(NAME qt6_real_dictionary_observer_unknown_option_rejection
    COMMAND qt6_real_dictionary_observer
      --dictionary-root unused-corpus
      --index-root unused-index
      --interface-language en_US
      --conditions-sha256 unused-hash
      --scenario clean-discovery
      --unknown unused-output)
  set_tests_properties(qt6_real_dictionary_observer_unknown_option_rejection
    PROPERTIES WILL_FAIL TRUE)

  ti_add_executable(qt6_real_mdict_observer
    tools/qt6_real_mdict_observer.cpp)
  ti_target_include_modules_recurse(qt6_real_mdict_observer
    goldendict_core)
  ti_target_link_libraries(qt6_real_mdict_observer
    goldendict_core ${QT_NAMESPACE}::Core)
  add_test(NAME qt6_real_mdict_observer_invalid_options
    COMMAND qt6_real_mdict_observer --unknown value)
  set_tests_properties(qt6_real_mdict_observer_invalid_options
    PROPERTIES WILL_FAIL TRUE)

  ti_add_executable(qt6_real_dsl_observer
    tools/qt6_real_dsl_observer.cpp)
  ti_target_include_modules_recurse(qt6_real_dsl_observer
    goldendict_core)
  ti_target_link_libraries(qt6_real_dsl_observer
    goldendict_core ${QT_NAMESPACE}::Core)
  add_test(NAME qt6_real_dsl_observer_invalid_options
    COMMAND qt6_real_dsl_observer --unknown value)
  set_tests_properties(qt6_real_dsl_observer_invalid_options
    PROPERTIES WILL_FAIL TRUE)

  ti_add_executable(qt6_real_lookup_observer
    tools/qt6_real_lookup_observer.cpp)
  ti_target_include_modules_recurse(qt6_real_lookup_observer
    goldendict_core)
  ti_target_link_libraries(qt6_real_lookup_observer
    goldendict_core ${QT_NAMESPACE}::Core)
  add_test(NAME qt6_real_lookup_observer_invalid_options
    COMMAND qt6_real_lookup_observer --unknown value)
  set_tests_properties(qt6_real_lookup_observer_invalid_options
    PROPERTIES WILL_FAIL TRUE)
endif()

get_target_property(core_interface_links goldendict_core
  INTERFACE_LINK_LIBRARIES)
if(core_interface_links AND core_interface_links MATCHES "OpenCC")
  message(FATAL_ERROR
    "OpenCC must remain private to the goldendict_core implementation")
endif()

if(TARGET sounddir_reader_test)
  target_sources(sounddir_reader_test PRIVATE
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/sounddir/sounddir_reader.cc
  )
  target_link_libraries(sounddir_reader_test PRIVATE ICU::uc)
endif()

if(TARGET sounddir_dictionary_test)
  target_sources(sounddir_dictionary_test PRIVATE
    src/dictionary/dictionary_backend.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/sounddir/sounddir_dictionary.cc
    src/formats/sounddir/sounddir_reader.cc
  )
  target_link_libraries(sounddir_dictionary_test PRIVATE ICU::uc)
endif()

if(TARGET zipsounds_discovery_test)
  target_sources(zipsounds_discovery_test PRIVATE
    src/formats/zipsounds/zipsounds_discovery.cc
  )
endif()

if(TARGET zipsounds_reader_test)
  target_sources(zipsounds_reader_test PRIVATE
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/zipsounds/zipsounds_reader.cc
  )
  target_link_libraries(zipsounds_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET zipsounds_dictionary_test)
  target_sources(zipsounds_dictionary_test PRIVATE
    src/dictionary/dictionary_backend.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/zipsounds/zipsounds_dictionary.cc
    src/formats/zipsounds/zipsounds_reader.cc
  )
  target_link_libraries(zipsounds_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET lsa_discovery_test)
  target_sources(lsa_discovery_test PRIVATE src/formats/lsa/lsa_discovery.cc)
  target_link_libraries(lsa_discovery_test
    PRIVATE Vorbis::vorbisenc Vorbis::vorbisfile
  )
endif()

if(TARGET lsa_reader_test)
  target_sources(lsa_reader_test PRIVATE
    src/audio/vorbis_decoder.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/lsa/lsa_reader.cc
  )
  target_link_libraries(lsa_reader_test
    PRIVATE ICU::uc Vorbis::vorbisenc Vorbis::vorbisfile
  )
endif()

if(TARGET lsa_dictionary_test)
  target_sources(lsa_dictionary_test PRIVATE
    src/audio/vorbis_decoder.cc
    src/dictionary/dictionary_backend.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/lsa/lsa_dictionary.cc
    src/formats/lsa/lsa_reader.cc
  )
  target_link_libraries(lsa_dictionary_test
    PRIVATE ICU::uc Vorbis::vorbisenc Vorbis::vorbisfile
  )
endif()

if(TARGET epwing_discovery_test)
  target_sources(epwing_discovery_test PRIVATE
    src/formats/epwing/epwing_discovery.cc
  )
endif()

if(TARGET epwing_reader_test)
  target_sources(epwing_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/epwing/epwing_reader.cc
  )
  target_link_libraries(epwing_reader_test PRIVATE ICU::uc)
endif()

if(TARGET epwing_dictionary_test)
  target_sources(epwing_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/epwing/epwing_dictionary.cc
    src/formats/epwing/epwing_reader.cc
  )
  target_link_libraries(epwing_dictionary_test PRIVATE expat::expat ICU::uc)
endif()

if(TARGET aard_reader_test)
  target_sources(aard_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/aard/aard_reader.cc
  )
  target_link_libraries(aard_reader_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET slob_discovery_test)
  target_sources(slob_discovery_test PRIVATE src/formats/slob/slob_discovery.cc)
endif()

if(TARGET slob_reader_test)
  target_sources(slob_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/slob/slob_reader.cc
  )
  target_link_libraries(slob_reader_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET slob_dictionary_test)
  target_sources(slob_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/slob/slob_dictionary.cc
    src/formats/slob/slob_reader.cc
  )
  target_link_libraries(slob_dictionary_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET zim_discovery_test)
  target_sources(zim_discovery_test PRIVATE src/formats/zim/zim_discovery.cc)
endif()

if(TARGET zim_reader_test)
  target_sources(zim_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/zim/zim_discovery.cc
    src/formats/zim/zim_reader.cc
  )
  target_link_libraries(zim_reader_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET zim_dictionary_test)
  target_sources(zim_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/zim/zim_dictionary.cc
    src/formats/zim/zim_discovery.cc
    src/formats/zim/zim_reader.cc
  )
  target_link_libraries(zim_dictionary_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET aard_discovery_test)
  target_sources(aard_discovery_test PRIVATE src/formats/aard/aard_discovery.cc)
endif()

if(TARGET aard_dictionary_test)
  target_sources(aard_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_index_lifecycle.cc
    src/dictionary/full_text_index_snapshot.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/aard/aard_dictionary.cc
    src/formats/aard/aard_reader.cc
  )
  target_link_libraries(aard_dictionary_test PRIVATE BZip2::BZip2 ICU::uc ZLIB::ZLIB)
endif()

if(TARGET mdict_reader_test)
  target_sources(mdict_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/mdict/mdict_discovery.cc
    src/formats/mdict/mdict_key_info_crypto.cc
    src/formats/mdict/mdict_reader.cc
  )
  target_link_libraries(mdict_reader_test
    PRIVATE expat::expat ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET mdict_discovery_test)
  target_sources(mdict_discovery_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/mdict/mdict_discovery.cc
    src/formats/mdict/mdict_key_info_crypto.cc
    src/formats/mdict/mdict_reader.cc
  )
  target_link_libraries(mdict_discovery_test
    PRIVATE expat::expat ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET mdict_dictionary_test)
  target_sources(mdict_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/mdict/mdict_dictionary.cc
    src/formats/mdict/mdict_discovery.cc
    src/formats/mdict/mdict_key_info_crypto.cc
    src/formats/mdict/mdict_reader.cc
  )
  target_link_libraries(mdict_dictionary_test
    PRIVATE expat::expat ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET bgl_reader_test)
  target_sources(bgl_reader_test PRIVATE
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/bgl/bgl_reader.cc
  )
  target_link_libraries(bgl_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET bgl_discovery_test)
  target_sources(bgl_discovery_test PRIVATE
    src/formats/bgl/bgl_discovery.cc
  )
endif()

if(TARGET bgl_dictionary_test)
  target_sources(bgl_dictionary_test PRIVATE
    src/article/article_assembler.cc
    src/article/article_document.cc
    src/article/internal_url.cc
    src/dictionary/dictionary_backend.cc
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/formats/bgl/bgl_dictionary.cc
    src/formats/bgl/bgl_reader.cc
  )
  target_link_libraries(bgl_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET article_assembler_test)
  target_sources(article_assembler_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
  )
endif()

if(TARGET article_composer_test)
  target_sources(article_composer_test PRIVATE
    src/article/article_composer.cc
    src/article/article_document.cc
  )
endif()

if(TARGET application_service_test)
  target_sources(application_service_test PRIVATE
    src/application/configuration_transaction_persistence.cc
    src/application/configuration_transaction_preparation.cc
    src/application/pending_configuration_transaction.cc
  )
  target_link_libraries(application_service_test
    PRIVATE BZip2::BZip2 Vorbis::vorbisenc Vorbis::vorbisfile ZLIB::ZLIB
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

if(TARGET transliteration_test)
  target_sources(transliteration_test PRIVATE
    src/dictionary/chinese_conversion.cc
    src/dictionary/transliteration.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
  )
  target_compile_definitions(transliteration_test PRIVATE
    "GOLDENDICT_OPENCC_TEST_CONFIG_DIRECTORY=\"${opencc_package_directories}/share/opencc\"")
  target_link_libraries(transliteration_test PRIVATE ICU::uc OpenCC::OpenCC)
endif()

if(TARGET hunspell_discovery_test)
  target_sources(hunspell_discovery_test PRIVATE
    src/morphology/hunspell_discovery.cc
  )
endif()

if(TARGET hunspell_content_test)
  target_sources(hunspell_content_test PRIVATE
    src/foundation/text_encoding.cc
    src/morphology/hunspell_content.cc
  )
  target_link_libraries(hunspell_content_test PRIVATE ICU::uc)
endif()

if(TARGET hunspell_provider_test)
  target_sources(hunspell_provider_test PRIVATE
    src/dictionary/dictionary_backend.cc
    src/foundation/text_encoding.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
    src/morphology/hunspell_content.cc
    src/morphology/hunspell_provider.cc
  )
  target_link_libraries(hunspell_provider_test
    PRIVATE hunspell::hunspell ICU::uc
  )
endif()

if(TARGET full_text_index_test)
  target_sources(full_text_index_test PRIVATE
    src/dictionary/full_text_index.cc
    src/dictionary/full_text_index_lifecycle.cc
    src/dictionary/full_text_index_snapshot.cc
    src/dictionary/full_text_index_work_executor.cc
    src/dictionary/full_text_matcher.cc
    src/dictionary/generated_index.cc
    src/foundation/text_folding.cc
    src/foundation/utf8.cc
  )
  target_link_libraries(full_text_index_test PRIVATE ICU::uc)
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
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
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
      src/dictionary/generated_index.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/dictd/dictd_reader.cc
  )
  target_link_libraries(dictd_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET dictd_dictionary_test)
  target_sources(dictd_dictionary_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
      src/dictionary/generated_index.cc
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
      src/dictionary/generated_index.cc
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
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
      src/dictionary/generated_index.cc
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
      src/dictionary/generated_index.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/xdxf/xdxf_reader.cc
  )
  target_link_libraries(xdxf_reader_test
    PRIVATE expat::expat ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET xdxf_dictionary_test)
  target_sources(xdxf_dictionary_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
      src/dictionary/generated_index.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/xdxf/xdxf_dictionary.cc
      src/formats/xdxf/xdxf_reader.cc
  )
  target_link_libraries(xdxf_dictionary_test
    PRIVATE expat::expat ICU::uc ZLIB::ZLIB
  )
endif()

if(TARGET gls_discovery_test)
  target_sources(gls_discovery_test
    PRIVATE
      src/formats/gls/gls_discovery.cc
  )
endif()

if(TARGET gls_reader_test)
  target_sources(gls_reader_test
    PRIVATE
      src/dictionary/generated_index.cc
      src/foundation/text_encoding.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/gls/gls_reader.cc
  )
  target_link_libraries(gls_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET gls_dictionary_test)
  target_sources(gls_dictionary_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
      src/dictionary/generated_index.cc
      src/foundation/text_encoding.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/gls/gls_dictionary.cc
      src/formats/gls/gls_reader.cc
  )
  target_link_libraries(gls_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET dsl_discovery_test)
  target_sources(dsl_discovery_test
    PRIVATE
      src/formats/dsl/dsl_discovery.cc
  )
endif()

if(TARGET dsl_reader_test)
  target_sources(dsl_reader_test
    PRIVATE
      src/dictionary/generated_index.cc
      src/foundation/text_encoding.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/dsl/dsl_abbreviation.cc
      src/formats/dsl/dsl_headword_parser.cc
      src/formats/dsl/dsl_reader.cc
  )
  target_link_libraries(dsl_reader_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()

if(TARGET dsl_dictionary_test)
  target_sources(dsl_dictionary_test
    PRIVATE
      src/article/article_assembler.cc
      src/article/article_document.cc
      src/article/internal_url.cc
      src/dictionary/dictionary_backend.cc
      src/dictionary/full_text_index.cc
      src/dictionary/full_text_matcher.cc
      src/dictionary/generated_index.cc
      src/foundation/text_encoding.cc
      src/foundation/text_folding.cc
      src/foundation/utf8.cc
      src/formats/dsl/dsl_abbreviation.cc
      src/formats/dsl/dsl_dictionary.cc
      src/formats/dsl/dsl_headword_parser.cc
      src/formats/dsl/dsl_reader.cc
      src/formats/dsl/dsl_resource_zip.cc
  )
  target_link_libraries(dsl_dictionary_test PRIVATE ICU::uc ZLIB::ZLIB)
endif()
