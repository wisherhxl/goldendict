#add_definitions(-D__OPENCV_BUILD=1)
#add_definitions(-D__OPENCV_APPS=1)

string(REPLACE "," ";" OPENCV_INSTALL_APPS_LIST "${OPENCV_INSTALL_APPS_LIST}")  # support comma-separated list (,) too

# link_libraries(${OPENCV_LINKER_LIBS})

# ti_glob_applications(${CMAKE_CURRENT_SOURCE_DIR})
