# ----------------------
# Find PCL from vcpkg
# set(CMAKE_PREFIX_PATH "$ENV{VCPKG_ROOT}/installed/x64-windows")
find_package(PCL REQUIRED)
# find_package(Eigen3 REQUIRED QUIET PATHS $ENV{VCPKG_ROOT}/installed NO_DEFAULT_PATH)
# find_package(lz4 REQUIRED QUIET PATHS $ENV{VCPKG_ROOT}/installed NO_DEFAULT_PATH)
# find_package(flann REQUIRED QUIET PATHS $ENV{VCPKG_ROOT}/installed NO_DEFAULT_PATH)
# find_package(PCL REQUIRED QUIET PATHS $ENV{VCPKG_ROOT}/packages NO_DEFAULT_PATH)

ti_list_components(PCL)