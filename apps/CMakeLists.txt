#add_definitions(-D__TIGER_BUILD=1)
#add_definitions(-D__TIGER_APPS=1)

string(REPLACE "," ";" TIGER_INSTALL_APPS_LIST "${TIGER_INSTALL_APPS_LIST}")  # support comma-separated list (,) too

# link_libraries(${TIGER_LINKER_LIBS})

ti_glob_applications(${CMAKE_CURRENT_SOURCE_DIR})
