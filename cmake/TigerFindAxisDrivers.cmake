# ----------------------
# Find Camera Drivers

set(axis_link
    acs
)
message(STATUS "Checking axis drivers...")
foreach(component ${axis_link})
    find_package(${component} CONFIG REQUIRED PATHS ${TI_DEV_PATH} NO_DEFAULT_PATH)
endforeach()

message(STATUS "======================================")
message(STATUS "Link axis_link to use the following axis drivers:")
foreach(component ${axis_link})
    message(STATUS "  ✓ ${component}")
endforeach()
message(STATUS "--------------------------------------")