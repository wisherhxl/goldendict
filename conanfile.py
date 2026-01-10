from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.errors import ConanException
from conan.tools.files import copy
import os
import re

class RagoRecipe(ConanFile):
    # Naming convention: all-lowercase project name used as internal CMake name (TI_INTERNAL_NAME)
    name = "rago"
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    revision_mode = "scm"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    def set_version(self):
        path = os.path.join(self.recipe_folder, "VERSION")
        with open(path, "r", encoding="utf-8") as f:
            ver = f.read().strip()

        if not re.fullmatch(r"\d+\.\d+\.\d+", ver):
            raise ConanException(f"VERSION must be X.Y.Z, got: {ver!r}")

        self.version = ver

    def requirements(self):
        self.requires("zlib/[>1.3]")
        self.requires("protobuf/[>3.0]")

    def build_requirements(self):
        self.tool_requires("cmake/[<3.27]")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/wisherhxl/tiger_template.git", target=".")
        # git.checkout("")
        git.run("submodule update --init --recursive")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.variables["INSTALL_PLATFORM_LAYOUT"] = False
        tc.variables["TIGER_SKIP_CMAKE_ROOT_CONFIG"] = True
        tc.variables["TIGER_ENABLE_STATIC_BINARIES_SUFFIX"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.builddirs = [f"lib/cmake/{self.name}"]
        self.cpp_info.set_property("cmake_find_mode", "none")
