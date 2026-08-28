cmake_minimum_required(VERSION 3.15)

project(opencc_package_test LANGUAGES CXX)

find_package(OpenCC CONFIG REQUIRED)

add_executable(opencc_package_test src/opencc_package_test.cpp)
target_compile_features(opencc_package_test PRIVATE cxx_std_17)
target_compile_definitions(
  opencc_package_test
  PRIVATE OPENCC_TEST_DATA_DIR="${OPENCC_TEST_DATA_DIR}"
)
target_link_libraries(opencc_package_test PRIVATE OpenCC::OpenCC)
