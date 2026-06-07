from conan import ConanFile
from conan.tools.scm import Git
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.env import VirtualRunEnv
from conan.errors import ConanException, ConanInvalidConfiguration
from conan.tools.files import copy
import os
import re

class TigerRecipe(ConanFile):
    # Naming convention: all-lowercase project name used as internal CMake name (TI_INTERNAL_NAME)
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    revision_mode = "scm"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "install_mode": ["library", "runtime"],
        "install_runtime_dependencies": ["auto", True, False],
        "qt_linux_platform_plugin": ["auto", "xcb", "wayland", "minimal", "off"],
        "qt_windows_platform_plugin": ["auto", "windows", "minimal", "off"],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
        "install_mode": "library",
        "install_runtime_dependencies": "auto",
        "qt_linux_platform_plugin": "auto",
        "qt_windows_platform_plugin": "auto",
        "qt/*:qttools": True,
    }

    def set_name(self):
        self.name = self._project_internal_name()

    def set_version(self):
        path = os.path.join(self.recipe_folder, "VERSION")
        with open(path, "r", encoding="utf-8") as f:
            ver = f.read().strip()

        if not re.fullmatch(r"\d+\.\d+\.\d+", ver):
            raise ConanException(f"VERSION must be X.Y.Z, got: {ver!r}")

        self.version = ver

    def requirements(self):
        self.requires("zlib/[>1.3]")
        self._public_requires("protobuf/6.33.5")
        self.requires("qt/5.15.16")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.26 <3.27]")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def validate(self):
        protobuf_shared = self._dependency_option("protobuf", "shared")
        abseil_shared = self._dependency_option("abseil", "shared")
        if (
            protobuf_shared is not None
            and abseil_shared is not None
            and not self._option_to_bool(protobuf_shared)
            and self._option_to_bool(abseil_shared)
        ):
            raise ConanInvalidConfiguration(
                "Static Protobuf with shared Abseil is unsupported. "
                "Use -o 'abseil/*:shared=False' when protobuf/*:shared=False, "
                "or use -o 'protobuf/*:shared=True'."
            )

    def layout(self):
        cmake_layout(self)

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/wisherhxl/tiger_template.git", target=".")
        # git.checkout("")
        git.run("submodule update --init --recursive")

    def _public_requires(self, reference):
        self.requires(
            reference,
            transitive_headers=True,
            transitive_libs=True,
        )

    def _project_internal_name(self):
        path = os.path.join(self.recipe_folder, "CMakeLists.txt")
        with open(path, "r", encoding="utf-8") as f:
            content = f.read()

        match = re.search(
            r'^\s*set\s*\(\s*TI_PROJECT_NAME\s+(?:"([^"]+)"|([^\s\)]+))\s*\)',
            content,
            re.MULTILINE,
        )
        if not match:
            raise ConanException("TI_PROJECT_NAME must be set in CMakeLists.txt")

        return (match.group(1) or match.group(2)).lower()

    def _dependency_option(self, dependency_name, option_name):
        for dependency in self.dependencies.host.values():
            if dependency.ref and dependency.ref.name == dependency_name:
                return dependency.options.get_safe(option_name)
        return None

    @staticmethod
    def _option_to_bool(option_value):
        return str(option_value).lower() in ("1", "true", "yes", "on")

    def _resolved_install_runtime_dependencies(self):
        install_runtime_dependencies = str(self.options.install_runtime_dependencies)
        if install_runtime_dependencies.lower() == "auto":
            return str(self.options.install_mode) == "runtime"
        return self._option_to_bool(install_runtime_dependencies)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_SHARED_LIBS"] = bool(self.options.shared)
        tc.variables["TIGER_INSTALL_CONAN_LAYOUT"] = True
        tc.variables["TIGER_INSTALL_MODE"] = str(self.options.install_mode)
        tc.variables["TIGER_QT_SHARED"] = bool(self.dependencies["qt"].options.shared)
        tc.variables["TIGER_PROTOBUF_SHARED"] = bool(
            self.dependencies["protobuf"].options.shared
        )
        if self.settings.os == "Linux":
            tc.variables["TIGER_QT_PLATFORM_PLUGIN"] = str(
                self.options.qt_linux_platform_plugin
            )
        elif self.settings.os == "Windows":
            tc.variables["TIGER_QT_PLATFORM_PLUGIN"] = str(
                self.options.qt_windows_platform_plugin
            )
        tc.variables["TIGER_INSTALL_RUNTIME_DEPENDENCIES"] = bool(
            self._resolved_install_runtime_dependencies()
        )
        qt_shared = self._dependency_option("qt", "shared")
        if qt_shared is not None:
            tc.variables["TIGER_QT_SHARED"] = self._option_to_bool(qt_shared)
        protobuf_shared = self._dependency_option("protobuf", "shared")
        if protobuf_shared is not None:
            tc.variables["TIGER_PROTOBUF_SHARED"] = self._option_to_bool(
                protobuf_shared
            )
        tc.generate()

    def build(self):
        cmake = CMake(self)
        with VirtualRunEnv(self).vars().apply():
            cmake.configure()
            cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.bindirs = ["bin"]
        if str(self.options.install_mode) == "runtime":
            self.cpp_info.includedirs = []
            self.cpp_info.libdirs = []
            self.cpp_info.builddirs = []
        else:
            self.cpp_info.builddirs = ["lib/cmake"]
        self.cpp_info.set_property("cmake_find_mode", "none")
