# SPDX-License-Identifier: GPL-3.0-or-later

set(the_description "GoldenDict optional network integrations")

ti_define_module(network
  INTERNAL
  REQUIRED PRIVATE EXTRAS Qt6::Network
)

if(TARGET http_client_test)
  target_sources(http_client_test PRIVATE src/http_client.cc)
  target_link_libraries(http_client_test PRIVATE Qt6::Network)
endif()
