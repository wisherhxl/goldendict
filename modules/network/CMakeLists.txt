# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict optional network integrations")
set(TIGER_MODULE_TYPE STATIC)

ti_define_module(network
  INTERNAL
  REQUIRED PUBLIC MODULES core
  REQUIRED PRIVATE MODULES external
  REQUIRED PRIVATE EXTRAS Qt6::Network
)

if(TARGET runtime_composition_test)
  target_sources(runtime_composition_test PRIVATE
    src/http_client.cc
    src/network_cache_storage.cc
    src/network_runtime.cc
    src/dict_server_source.cc
    src/forvo_source.cc
    src/mediawiki_source.cc
    src/runtime_composition.cc
    src/website_source.cc
  )
  target_link_libraries(runtime_composition_test PRIVATE
    goldendict_external
    Qt6::Network
  )
endif()

if(TARGET http_client_test)
  target_sources(http_client_test PRIVATE
    src/http_client.cc
    src/network_cache_storage.cc
    src/network_runtime.cc
  )
  target_link_libraries(http_client_test PRIVATE Qt6::Network)
endif()

if(TARGET mediawiki_source_test)
  target_sources(mediawiki_source_test PRIVATE
    src/http_client.cc
    src/network_cache_storage.cc
    src/network_runtime.cc
    src/mediawiki_source.cc
  )
  target_link_libraries(mediawiki_source_test PRIVATE Qt6::Network)
endif()

if(TARGET website_source_test)
  target_sources(website_source_test PRIVATE
    src/http_client.cc
    src/network_cache_storage.cc
    src/network_runtime.cc
    src/website_source.cc
  )
  target_link_libraries(website_source_test PRIVATE Qt6::Network)
endif()

if(TARGET forvo_source_test)
  target_sources(forvo_source_test PRIVATE
    src/forvo_source.cc
    src/http_client.cc
    src/network_cache_storage.cc
    src/network_runtime.cc
  )
  target_link_libraries(forvo_source_test PRIVATE Qt6::Network)
endif()

if(TARGET dict_server_source_test)
  target_sources(dict_server_source_test PRIVATE src/dict_server_source.cc)
  target_link_libraries(dict_server_source_test PRIVATE Qt6::Network)
endif()
