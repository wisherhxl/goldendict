
# ----------------------------------------------------------------------------
#  Check if DEV2022 is installed
# ----------------------------------------------------------------------------
set(TI_DEV_TOOLS "DEV2022")
if(NOT DEFINED ENV{${TI_DEV_TOOLS}})
    message(FATAL_ERROR "Can't find ${TI_DEV_TOOLS}, please install it first.")
endif()

set(TI_DEV_PATH $ENV{${TI_DEV_TOOLS}})
message(STATUS "Found ${TI_DEV_TOOLS} in ${TI_DEV_PATH}")