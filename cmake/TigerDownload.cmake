#
#  Download and optionally unpack a file
#
#  ti_download(FILENAME p HASH h URL u1 [u2 ...] DESTINATION_DIR d [ID id] [STATUS s] [UNPACK] [RELATIVE_URL])
#    FILENAME - filename
#    HASH - MD5 hash
#    URL - full download url (first nonempty value will be chosen)
#    DESTINATION_DIR - file will be copied to this directory
#    ID     - identifier for project/group of downloaded files
#    STATUS - passed variable will be updated in parent scope,
#             function will not fail the build in case of download problem if this option is provided,
#             but will fail in case when other operations (copy, remove, etc.) failed
#    UNPACK - downloaded file will be unpacked to DESTINATION_DIR
#    RELATIVE_URL - if set, then URL is treated as a base, and FILENAME will be appended to it
#  Note: uses TIGER_DOWNLOAD_PATH folder as cache, default is <tiger>/.cache

set(HELP_TIGER_DOWNLOAD_PATH "Cache directory for downloaded files")
if(DEFINED ENV{TIGER_DOWNLOAD_PATH})
  set(TIGER_DOWNLOAD_PATH "$ENV{TIGER_DOWNLOAD_PATH}" CACHE PATH "${HELP_TIGER_DOWNLOAD_PATH}")
endif()
set(TIGER_DOWNLOAD_PATH "${${PROJECT_NAME}_SOURCE_DIR}/.cache" CACHE PATH "${HELP_TIGER_DOWNLOAD_PATH}")
set(TIGER_DOWNLOAD_LOG "${${PROJECT_NAME}_BINARY_DIR}/CMakeDownloadLog.txt")
set(TIGER_DOWNLOAD_WITH_CURL "${${PROJECT_NAME}_BINARY_DIR}/download_with_curl.sh")
set(TIGER_DOWNLOAD_WITH_WGET "${${PROJECT_NAME}_BINARY_DIR}/download_with_wget.sh")
set(TIGER_DOWNLOAD_TRIES_LIST 1 CACHE STRING "List of download tries") # a list
set(TIGER_DOWNLOAD_PARAMS INACTIVITY_TIMEOUT 60 TIMEOUT 600 CACHE STRING "Download parameters to be passed to file(DOWNLOAD ...)")
mark_as_advanced(TIGER_DOWNLOAD_TRIES_LIST TIGER_DOWNLOAD_PARAMS)

# Init download cache directory and log file and helper scripts
if(NOT EXISTS "${TIGER_DOWNLOAD_PATH}")
  file(MAKE_DIRECTORY ${TIGER_DOWNLOAD_PATH})
endif()
if(NOT EXISTS "${TIGER_DOWNLOAD_PATH}/.gitignore")
  file(WRITE "${TIGER_DOWNLOAD_PATH}/.gitignore" "*\n")
endif()
file(WRITE "${TIGER_DOWNLOAD_LOG}" "#use_cache \"${TIGER_DOWNLOAD_PATH}\"\n")
file(REMOVE "${TIGER_DOWNLOAD_WITH_CURL}")
file(REMOVE "${TIGER_DOWNLOAD_WITH_WGET}")

ti_check_environment_variables(TIGER_DOWNLOAD_MIRROR_ID)

function(ti_init_download_mirror)
  if(NOT GIT_FOUND)
    return()
  endif()
  if(NOT DEFINED TIGER_DOWNLOAD_MIRROR_ID)
    # Run `git remote get-url origin` to get remote source
    execute_process(
      COMMAND
        ${GIT_EXECUTABLE} remote get-url origin
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE
        RESULT_STATUS
      OUTPUT_VARIABLE
        TI_GIT_ORIGIN_URL_OUTPUT
      ERROR_QUIET
    )
    # if non-git, TI_GIT_ORIGIN_URL_OUTPUT is empty
    if(NOT TI_GIT_ORIGIN_URL_OUTPUT)
      message(STATUS "ti_init_download: Tiger source tree is not fetched as git repository. 3rdparty resources will be downloaded from github.com by default.")
      return()
    else()
      # Check if git origin is github.com
      string(FIND "${TI_GIT_ORIGIN_URL_OUTPUT}" "github.com" _found_github)
      if(NOT ${_found_github} EQUAL -1)
        set(TIGER_DOWNLOAD_MIRROR_ID "github" CACHE STRING "")
      endif()
      # Check if git origin is gitcode.net
      string(FIND "${TI_GIT_ORIGIN_URL_OUTPUT}" "gitcode.net" _found_gitcode)
      if(NOT ${_found_gitcode} EQUAL -1)
        set(TIGER_DOWNLOAD_MIRROR_ID "gitcode" CACHE STRING "")
      endif()
    endif()
  endif()

  if(TIGER_DOWNLOAD_MIRROR_ID STREQUAL "gitcode" OR TIGER_DOWNLOAD_MIRROR_ID STREQUAL "custom")
    message(STATUS "ti_init_download: Using ${TIGER_DOWNLOAD_MIRROR_ID}-hosted mirror to download 3rdparty components.")
    ti_cmake_hook_append(TIGER_DOWNLOAD_PRE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mirrors/${TIGER_DOWNLOAD_MIRROR_ID}.cmake")
  elseif(TIGER_DOWNLOAD_MIRROR_ID STREQUAL "github")
    return()
  else()
    message(STATUS "ti_init_download: Unable to recognize git server of Tiger source code. Using github.com to download 3rdparty components.")
  endif()
endfunction()

function(ti_download)
  cmake_parse_arguments(DL "UNPACK;RELATIVE_URL" "FILENAME;HASH;DESTINATION_DIR;ID;STATUS" "URL" ${ARGN})

  function(ti_download_log)
    file(APPEND "${TIGER_DOWNLOAD_LOG}" "${ARGN}\n")
  endfunction()

  ti_assert(DL_FILENAME)
  ti_assert(DL_HASH)
  ti_assert(DL_URL)
  ti_assert(DL_DESTINATION_DIR)
  if((NOT " ${DL_UNPARSED_ARGUMENTS}" STREQUAL " ")
    OR DL_FILENAME STREQUAL ""
    OR DL_HASH STREQUAL ""
    OR DL_URL STREQUAL ""
    OR DL_DESTINATION_DIR STREQUAL ""
  )
    set(msg_level FATAL_ERROR)
    if(DEFINED DL_STATUS)
      set(${DL_STATUS} FALSE PARENT_SCOPE)
      set(msg_level WARNING)
    endif()
    message(${msg_level} "ERROR: ti_download() unsupported arguments: ${ARGV}")
    return()
  endif()

  if(DEFINED DL_STATUS)
    set(${DL_STATUS} TRUE PARENT_SCOPE)
  endif()

  ti_cmake_hook(TIGER_DOWNLOAD_PRE)

  # Check CMake cache for already processed tasks
  string(FIND "${DL_DESTINATION_DIR}" "${CMAKE_BINARY_DIR}" DL_BINARY_PATH_POS)
  if(DL_BINARY_PATH_POS EQUAL 0)
    set(__file_id "${DL_DESTINATION_DIR}/${DL_FILENAME}")
    file(RELATIVE_PATH __file_id "${CMAKE_BINARY_DIR}" "${__file_id}")
    string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" __file_id "${__file_id}")
    if(DL_ID)
      string(TOUPPER ${DL_ID} __id)
      string(REGEX REPLACE "[^a-zA-Z0-9_]" "_" __id "${__id}")
      set(TI_DOWNLOAD_HASH_NAME "TI_DOWNLOAD_${__id}_HASH_${__file_id}")
    else()
      set(TI_DOWNLOAD_HASH_NAME "TI_DOWNLOAD_HASH_${__file_id}")
    endif()
    if(" ${${TI_DOWNLOAD_HASH_NAME}}" STREQUAL " ${DL_HASH}")
      ti_download_log("#match_hash_in_cmake_cache \"${TI_DOWNLOAD_HASH_NAME}\"")
      return()
    endif()
    unset("${TI_DOWNLOAD_HASH_NAME}" CACHE)
  else()
    set(TI_DOWNLOAD_HASH_NAME "")
    #message(WARNING "Download destination is not in CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}: ${DL_DESTINATION_DIR}")
  endif()

  # Select first non-empty url
  foreach(url ${DL_URL})
    if(url)
      set(DL_URL "${url}")
      break()
    endif()
  endforeach()

  # Append filename to url if needed
  if(DL_RELATIVE_URL)
    set(DL_URL "${DL_URL}${DL_FILENAME}")
  endif()

  set(mode "copy")
  if(DL_UNPACK)
    set(mode "unpack")
  endif()

  # Log all calls to file
  ti_download_log("#do_${mode} \"${DL_FILENAME}\" \"${DL_HASH}\" \"${DL_URL}\" \"${DL_DESTINATION_DIR}\"")
  # ... and to console
  set(__msg_prefix "")
  if(DL_ID)
    set(__msg_prefix "${DL_ID}: ")
  endif()
  message(STATUS "${__msg_prefix}Downloading ${DL_FILENAME} from ${DL_URL}")

  # Copy mode: check if copy destination exists and is correct
  if(NOT DL_UNPACK)
    set(COPY_DESTINATION "${DL_DESTINATION_DIR}/${DL_FILENAME}")
    if(EXISTS "${COPY_DESTINATION}")
      ti_download_log("#check_md5 \"${COPY_DESTINATION}\"")
      file(MD5 "${COPY_DESTINATION}" target_md5)
      if(target_md5 STREQUAL DL_HASH)
        ti_download_log("#match_md5 \"${COPY_DESTINATION}\" \"${target_md5}\"")
        if(TI_DOWNLOAD_HASH_NAME)
          set(${TI_DOWNLOAD_HASH_NAME} "${DL_HASH}" CACHE INTERNAL "")
        endif()
        return()
      endif()
      ti_download_log("#mismatch_md5 \"${COPY_DESTINATION}\" \"${target_md5}\"")
    else()
      ti_download_log("#missing \"${COPY_DESTINATION}\"")
    endif()
  endif()

  # Check cache first
  if(DL_ID)
    string(TOLOWER "${DL_ID}" __id)
    string(REGEX REPLACE "[^a-zA-Z0-9_/ ]" "_" __id "${__id}")
    set(CACHE_CANDIDATE "${TIGER_DOWNLOAD_PATH}/${__id}/${DL_HASH}-${DL_FILENAME}")
  else()
    set(CACHE_CANDIDATE "${TIGER_DOWNLOAD_PATH}/${DL_HASH}-${DL_FILENAME}")
  endif()
  if(EXISTS "${CACHE_CANDIDATE}")
    ti_download_log("#check_md5 \"${CACHE_CANDIDATE}\"")
    file(MD5 "${CACHE_CANDIDATE}" target_md5)
    if(NOT target_md5 STREQUAL DL_HASH)
      ti_download_log("#mismatch_md5 \"${CACHE_CANDIDATE}\" \"${target_md5}\"")
      ti_download_log("#delete \"${CACHE_CANDIDATE}\"")
      file(REMOVE ${CACHE_CANDIDATE})
    endif()
  endif()

  # Download
  if(NOT EXISTS "${CACHE_CANDIDATE}")
    ti_download_log("#cmake_download \"${CACHE_CANDIDATE}\" \"${DL_URL}\"")
    foreach(try ${TIGER_DOWNLOAD_TRIES_LIST})
      ti_download_log("#try ${try}")
      file(DOWNLOAD "${DL_URL}" "${CACHE_CANDIDATE}"
           STATUS status
           LOG __log
           ${TIGER_DOWNLOAD_PARAMS})
      if(status EQUAL 0)
        break()
      endif()
      message(STATUS "Try ${try} failed")
    endforeach()
    if(NOT TIGER_SKIP_FILE_DOWNLOAD_DUMP)  # workaround problem with old CMake versions: "Invalid escape sequence"
      string(LENGTH "${__log}" __log_length)
      if(__log_length LESS 65536)
        string(REPLACE "\n" "\n# " __log "${__log}")
        ti_download_log("# ${__log}\n")
      endif()
    endif()
    if(NOT status EQUAL 0)
      set(msg_level FATAL_ERROR)
      if(DEFINED DL_STATUS)
        set(${DL_STATUS} FALSE PARENT_SCOPE)
        set(msg_level WARNING)
      endif()
      if(status MATCHES "Couldn't resolve host name")
        message(STATUS "
=======================================================================
  Couldn't download files from the Internet.
  Please check the Internet access on this host.
=======================================================================
")
      elseif(status MATCHES "Couldn't connect to server")
        message(STATUS "
=======================================================================
  Couldn't connect to server from the Internet.
  Perhaps direct connections are not allowed in the current network.
  To use proxy please check/specify these environment variables:
  - http_proxy/https_proxy
  - and/or HTTP_PROXY/HTTPS_PROXY
=======================================================================
")
      endif()
      message(${msg_level} "${__msg_prefix}Download failed: ${status}
For details please refer to the download log file:
${TIGER_DOWNLOAD_LOG}
")
      # write helper scripts for failed downloads
      file(APPEND "${TIGER_DOWNLOAD_WITH_CURL}" "curl --create-dirs --output \"${CACHE_CANDIDATE}\" \"${DL_URL}\"\n")
      file(APPEND "${TIGER_DOWNLOAD_WITH_WGET}" "mkdir -p $(dirname ${CACHE_CANDIDATE}) && wget -O \"${CACHE_CANDIDATE}\" \"${DL_URL}\"\n")
      return()
    endif()

    # Don't remove this code, because EXPECTED_MD5 parameter doesn't fail "file(DOWNLOAD)" step on wrong hash
    ti_download_log("#check_md5 \"${CACHE_CANDIDATE}\"")
    file(MD5 "${CACHE_CANDIDATE}" target_md5)
    if(NOT target_md5 STREQUAL DL_HASH)
      ti_download_log("#mismatch_md5 \"${CACHE_CANDIDATE}\" \"${target_md5}\"")
      set(msg_level FATAL_ERROR)
      if(DEFINED DL_STATUS)
        set(${DL_STATUS} FALSE PARENT_SCOPE)
        set(msg_level WARNING)
      endif()
      message(${msg_level} "${__msg_prefix}Hash mismatch: ${target_md5}")
      return()
    endif()
  endif()

  # Unpack or copy
  if(DL_UNPACK)
    if(EXISTS "${DL_DESTINATION_DIR}")
      ti_download_log("#remove_unpack \"${DL_DESTINATION_DIR}\"")
      file(REMOVE_RECURSE "${DL_DESTINATION_DIR}")
    endif()
    ti_download_log("#mkdir \"${DL_DESTINATION_DIR}\"")
    file(MAKE_DIRECTORY "${DL_DESTINATION_DIR}")
    ti_download_log("#unpack \"${DL_DESTINATION_DIR}\" \"${CACHE_CANDIDATE}\"")
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${CACHE_CANDIDATE}"
                    WORKING_DIRECTORY "${DL_DESTINATION_DIR}"
                    RESULT_VARIABLE res)
    if(NOT res EQUAL 0)
      message(FATAL_ERROR "${__msg_prefix}Unpack failed: ${res}")
    endif()
  else()
    ti_download_log("#copy \"${COPY_DESTINATION}\" \"${CACHE_CANDIDATE}\"")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CACHE_CANDIDATE}" "${COPY_DESTINATION}"
                    RESULT_VARIABLE res)
    if(NOT res EQUAL 0)
      message(FATAL_ERROR "${__msg_prefix}Copy failed: ${res}")
    endif()
  endif()

  if(TI_DOWNLOAD_HASH_NAME)
    set(${TI_DOWNLOAD_HASH_NAME} "${DL_HASH}" CACHE INTERNAL "")
  endif()
endfunction()

# ----------------------------------------------------------------------------
#  Initialize download in case mirror is used
# ----------------------------------------------------------------------------
ti_init_download_mirror()
