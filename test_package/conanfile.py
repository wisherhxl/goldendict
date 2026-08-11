from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.files import save
import os
import glob


class TestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"
    test_type = "explicit"
    exports_sources = "headless_api_test.cpp"

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires(self.tested_reference_str)

    def generate(self):
        tested_dep = None
        for dep in self.dependencies.host.values():
            if str(dep.ref) == self.tested_reference_str:
                tested_dep = dep
                break
        if tested_dep is None:
            tested_dep = next(iter(self.dependencies.host.values()))

        config_files = glob.glob(
            os.path.join(tested_dep.package_folder, "**", "*Config.cmake"),
            recursive=True,
        )
        config_files = [
            path for path in config_files
            if not path.endswith("ConfigVersion.cmake")
            and not path.endswith("Config-version.cmake")
        ]
        config_files.sort()
        if not config_files:
            raise RuntimeError(
                f"Could not find an installed CMake package config in {tested_dep.package_folder}"
            )

        version_headers = glob.glob(
            os.path.join(tested_dep.package_folder, "**", "*_version.tp.h"),
            recursive=True,
        )
        version_headers.sort()
        if not version_headers:
            raise RuntimeError(
                f"Could not find an installed *_version.tp.h header in {tested_dep.package_folder}"
            )

        version_header = version_headers[0]
        version_header_name = os.path.basename(version_header)
        namespace = version_header_name[:-len("_version.tp.h")]
        macro_prefix = namespace.upper()

        include_dirs = []
        for include_dir in tested_dep.cpp_info.includedirs:
            if os.path.isabs(include_dir):
                include_dirs.append(include_dir)
            else:
                include_dirs.append(os.path.join(tested_dep.package_folder, include_dir))
        include_dirs.append(tested_dep.package_folder)

        version_header_include = None
        for include_dir in include_dirs:
            try:
                relpath = os.path.relpath(version_header, include_dir)
            except ValueError:
                continue
            if not relpath.startswith(".."):
                version_header_include = relpath.replace(os.sep, "/")
                break
        if version_header_include is None:
            raise RuntimeError(f"Could not compute include path for {version_header}")

        package_name = os.path.basename(config_files[0])[:-len("Config.cmake")]
        save(
            self,
            os.path.join(self.source_folder, "CMakeLists.txt"),
            f"""cmake_minimum_required(VERSION 3.15)
project(test_package LANGUAGES C CXX)

if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "{self.settings.build_type}" CACHE STRING "" FORCE)
endif()

find_package({package_name} CONFIG REQUIRED)

add_executable(test_package test_package.c)
target_link_libraries(test_package PRIVATE ${{{package_name}_LIBS}})

add_executable(headless_api_test headless_api_test.cpp)
target_compile_features(headless_api_test PRIVATE cxx_std_17)
target_link_libraries(headless_api_test PRIVATE goldendict::core)
""",
        )
        save(
            self,
            os.path.join(self.source_folder, "test_package.c"),
            f"""#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <{version_header_include}>

#define TESTED_PROJECT_NAME {macro_prefix}_PROJECT_NAME
#define TESTED_VERSION {macro_prefix}_VERSION

int main(void) {{
    const char *project_name = TESTED_PROJECT_NAME;
    const char *project_info = strstr(project_name, " - ");

    if (project_info != NULL) {{
        printf("test_package: %.*s %s\\n",
               (int)(project_info - project_name),
               project_name,
               TESTED_VERSION);
        printf("              %s\\n", project_info + 3);
    }} else {{
        printf("test_package: %s %s\\n", project_name, TESTED_VERSION);
    }}
    printf("              linked successfully\\n");
    return EXIT_SUCCESS;
}}
""",
        )
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def test(self):
        if can_run(self):
            bin_path = os.path.join(self.cpp.build.bindirs[0], "test_package")
            self.run(bin_path, env="conanrun")
            headless_path = os.path.join(
                self.cpp.build.bindirs[0], "headless_api_test"
            )
            self.run(headless_path, env="conanrun")
