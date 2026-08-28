import os

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout


class OpenCCTestPackage(ConanFile):
    test_type = "explicit"
    settings = "os", "compiler", "build_type", "arch"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["CMAKE_PREFIX_PATH"] = self.dependencies[
            "opencc"
        ].package_folder.replace("\\", "/")
        toolchain.variables["OPENCC_TEST_DATA_DIR"] = os.path.join(
            self.dependencies["opencc"].package_folder, "share", "opencc"
        ).replace("\\", "/")
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            self.run(
                os.path.join(self.cpp.build.bindir, "opencc_package_test"),
                env="conanrun",
            )
