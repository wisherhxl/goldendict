# ----------------------------------------------------------------------------
#   Uninstall target, for "make uninstall"
# ----------------------------------------------------------------------------
if(NOT TARGET uninstall)  # avoid conflicts with parent projects
  configure_file(
      "${Tiger_SOURCE_DIR}/cmake/templates/cmake_uninstall.cmake.in"
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
add_custom_target(tiger_modules)
if(ENABLE_SOLUTION_FOLDERS)
  set_target_properties(tiger_modules PROPERTIES FOLDER "extra")
endif()


# ----------------------------------------------------------------------------
# targets building all tests
# ----------------------------------------------------------------------------
if(BUILD_TESTS)
  add_custom_target(tiger_tests)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(tiger_tests PROPERTIES FOLDER "extra")
  endif()
endif()
if(BUILD_PERF_TESTS)
  add_custom_target(tiger_perf_tests)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(tiger_perf_tests PROPERTIES FOLDER "extra")
  endif()
endif()

# Documentation
if(BUILD_DOCS)
  add_custom_target(tiger_docs)
  add_custom_target(install_docs DEPENDS tiger_docs
    COMMAND "${CMAKE_COMMAND}" -DCMAKE_INSTALL_COMPONENT=docs -P "${CMAKE_BINARY_DIR}/cmake_install.cmake")
endif()

# Samples
if(BUILD_EXAMPLES)
  add_custom_target(tiger_samples)
  if(ENABLE_SOLUTION_FOLDERS)
    set_target_properties(tiger_samples PROPERTIES FOLDER "extra")
  endif()
endif()
