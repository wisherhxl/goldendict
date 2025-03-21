# ----------------------------------------------------------------------------
#   Uninstall target, for "make uninstall"
# ----------------------------------------------------------------------------
if(NOT TARGET uninstall)  # avoid conflicts with parent projects
  configure_file(
      "${${PROJECT_NAME}_SOURCE_DIR}/cmake/templates/cmake_uninstall.cmake.in"
      "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
      @ONLY
  )

  add_custom_target(uninstall
      COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  )

  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(uninstall PROPERTIES FOLDER "CMakeTargets")
  endif()
endif()

# ----------------------------------------------------------------------------
# target building all Tiger modules
# ----------------------------------------------------------------------------
add_custom_target(${TI_INTERNAL_NAME}_modules)
if(ENABLE_SOLUTION_FOLDERS)
  set_target_properties(${TI_INTERNAL_NAME}_modules PROPERTIES FOLDER "extra")
endif()


# ----------------------------------------------------------------------------
# targets building all tests
# ----------------------------------------------------------------------------
if(BUILD_TESTS)
  add_custom_target(${TI_INTERNAL_NAME}_tests)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(${TI_INTERNAL_NAME}_tests PROPERTIES FOLDER "extra")
  endif()
endif()
if(BUILD_PERF_TESTS)
  add_custom_target(${TI_INTERNAL_NAME}_perf_tests)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(${TI_INTERNAL_NAME}_perf_tests PROPERTIES FOLDER "extra")
  endif()
endif()

# Documentation
if(BUILD_DOCS)
  add_custom_target(${TI_INTERNAL_NAME}_docs)
  add_custom_target(install_docs DEPENDS ${TI_INTERNAL_NAME}_docs
    COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT=docs -P "${CMAKE_BINARY_DIR}/cmake_install.cmake")
endif()

# Samples
if(BUILD_EXAMPLES)
  add_custom_target(${TI_INTERNAL_NAME}_samples)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(${TI_INTERNAL_NAME}_samples PROPERTIES FOLDER "extra")
  endif()
endif()
