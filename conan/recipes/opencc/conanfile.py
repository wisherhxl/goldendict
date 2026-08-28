import os
import sys

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, download, get, replace_in_file, rm


class OpenCCRecipe(ConanFile):
    name = "opencc"
    version = "1.4.1"
    package_type = "library"
    license = (
        "Apache-2.0",
        "BSD-2-Clause OR LGPL-2.1-or-later",
        "BSD-3-Clause",
        "MIT",
    )
    homepage = "https://github.com/BYVoid/OpenCC"
    description = "Conversion between Traditional and Simplified Chinese"
    topics = ("chinese", "text-conversion", "localization")

    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": True, "fPIC": True}

    _runtime_configs = ("s2hk.json", "s2tw.json", "t2s.json")
    _runtime_dictionaries = (
        "CJK_Compatibility_Ideographs.ocd2",
        "HKVariants.ocd2",
        "HKVariantsPhrases.ocd2",
        "STCharacters.ocd2",
        "STPhrases.ocd2",
        "STPhrases_GeneratedFromRegionalPhrases.ocd2",
        "TSCharacters.ocd2",
        "TSCharactersExt.ocd2",
        "TSPhrases.ocd2",
        "TWVariants.ocd2",
        "TWVariantsPhrases.ocd2",
    )

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def source(self):
        get(self, **self.conan_data["sources"][self.version], strip_root=True)
        download(
            self,
            **self.conan_data["notices"]["rapidjson"],
            filename=os.path.join(self.source_folder, "rapidjson-license.txt"),
        )
        # Release archives contain no Git metadata and 1.4.1 retained a 1.4.0
        # fallback, so align the installed library/config version with the tag.
        replace_in_file(
            self,
            os.path.join(self.source_folder, "cmake", "GitVersion.cmake"),
            "set(_OPENCC_FALLBACK_REVISION 0)",
            "set(_OPENCC_FALLBACK_REVISION 1)",
        )
        # Preserve upstream's config export while fixing its installed target's
        # include root for the documented <opencc/...> include spelling.
        replace_in_file(
            self,
            os.path.join(self.source_folder, "src", "CMakeLists.txt"),
            "$<INSTALL_INTERFACE:${DIR_INCLUDE}/opencc>",
            "$<INSTALL_INTERFACE:${DIR_INCLUDE}>",
        )

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.variables["BUILD_SHARED_LIBS"] = self.options.shared
        toolchain.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe(
            "fPIC", True
        )
        toolchain.variables["OPENCC_ENABLE_INSTALL"] = True
        toolchain.variables["BUILD_DOCUMENTATION"] = False
        toolchain.variables["ENABLE_GTEST"] = False
        toolchain.variables["ENABLE_BENCHMARK"] = False
        toolchain.variables["BUILD_OPENCC_JIEBA_PLUGIN"] = False
        toolchain.variables["BUILD_PYTHON"] = False
        toolchain.variables["OPENCC_DICT_FORMAT"] = "ocd2"
        toolchain.variables["Python3_EXECUTABLE"] = sys.executable.replace("\\", "/")
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

        data_dir = os.path.join(self.package_folder, "share", "opencc")
        retained_data = self._runtime_configs + self._runtime_dictionaries
        for filename in os.listdir(data_dir):
            if filename not in retained_data:
                rm(self, filename, data_dir)

        bin_dir = os.path.join(self.package_folder, "bin")
        for executable in (
            "opencc",
            "opencc.exe",
            "opencc_dict",
            "opencc_dict.exe",
            "opencc_phrase_extract",
            "opencc_phrase_extract.exe",
        ):
            rm(self, executable, bin_dir)
        rm(self, "*.pc", os.path.join(self.package_folder, "lib", "pkgconfig"))

        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses", "opencc"),
        )
        copy(
            self,
            "COPYING.md",
            src=os.path.join(self.source_folder, "deps", "marisa-0.3.1"),
            dst=os.path.join(self.package_folder, "licenses", "marisa-trie"),
        )
        copy(
            self,
            "COPYING.md",
            src=os.path.join(self.source_folder, "deps", "darts-clone-0.32h"),
            dst=os.path.join(self.package_folder, "licenses", "darts-clone"),
        )
        copy(
            self,
            "rapidjson-license.txt",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses", "rapidjson"),
        )

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "OpenCC")
        self.cpp_info.set_property("cmake_target_name", "OpenCC::OpenCC")
        self.cpp_info.libs = ["opencc"]
        self.cpp_info.resdirs = [os.path.join("share", "opencc")]
        if not self.options.shared:
            self.cpp_info.libs.append("marisa")
            self.cpp_info.defines = ["Opencc_BUILT_AS_STATIC"]
