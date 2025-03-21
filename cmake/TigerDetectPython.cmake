find_package(PythonInterp REQUIRED)

if(PythonInterp_FOUND)
  message(STATUS "Python interpreter found: ${PYTHON_EXECUTABLE}")
  # You can now use ${PYTHON_EXECUTABLE} to run Python scripts
else()
  message(FATAL_ERROR "Python interpreter not found.")
endif()