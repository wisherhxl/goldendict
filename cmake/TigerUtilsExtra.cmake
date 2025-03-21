# Creates a directory and checks if the creation was successful.
function(ti_create_directory directory_path)
  file(MAKE_DIRECTORY "${directory_path}")

  if(NOT EXISTS "${directory_path}")
    message(FATAL_ERROR "Failed to create directory: ${directory_path}")
  else()
    message(STATUS "Directory created successfully: ${directory_path}")
  endif()
endfunction()

# Get the current year and month.
function(ti_get_date_components year month)
  execute_process(
    COMMAND
      python -c
      "import datetime; now = datetime.datetime.now(); print(now.year); print(now.month)"
    OUTPUT_VARIABLE DATE_OUTPUT
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  string(REGEX REPLACE "([0-9]+)\n([0-9]+)" "\\1;\\2" DATE_OUTPUT
                       "${DATE_OUTPUT}")

  list(GET DATE_OUTPUT 0 DATE_YEAR)
  list(GET DATE_OUTPUT 1 DATE_MONTH)
  set(${year} ${DATE_YEAR} PARENT_SCOPE)
  set(${month} ${DATE_MONTH} PARENT_SCOPE)
endfunction()
