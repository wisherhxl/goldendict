# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict headless and desktop application core")

ti_define_module(core REQUIRED PUBLIC MODULES tiger)

if(TARGET stardict_reader_test)
  target_sources(stardict_reader_test
    PRIVATE
      src/formats/stardict/stardict_reader.cc
  )
endif()

if(TARGET stardict_dictionary_test)
  target_sources(stardict_dictionary_test
    PRIVATE
      src/dictionary/dictionary_backend.cc
      src/formats/stardict/stardict_dictionary.cc
      src/formats/stardict/stardict_reader.cc
  )
endif()

if(TARGET stardict_discovery_test)
  target_sources(stardict_discovery_test
    PRIVATE
      src/formats/stardict/stardict_discovery.cc
  )
endif()
