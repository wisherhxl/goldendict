# ----------------------
# Find Camera Drivers

set(camera_link
    basler
)
message(STATUS "Checking camera drivers...")
foreach(component ${camera_link})
    find_package(${component} CONFIG REQUIRED PATHS ${TI_DEV_PATH} NO_DEFAULT_PATH)
endforeach()

message(STATUS "======================================")
message(STATUS "Link camera_link to use the following camera drivers:")
foreach(component ${camera_link})
    message(STATUS "  ✓ ${component}")
endforeach()
message(STATUS "--------------------------------------")