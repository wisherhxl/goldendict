import os

from conan import ConanFile


class QtLibalsaBuildenvRecipe(ConanFile):
    name = "qt-libalsa-buildenv"
    version = "1.2.10"
    package_type = "application"
    license = "MIT"

    def requirements(self):
        self.requires("libalsa/1.2.10")

    def package_info(self):
        libalsa = self.dependencies.host["libalsa"]
        for include_dir in libalsa.cpp_info.includedirs:
            self.buildenv_info.append_path(
                "CPATH", os.path.join(libalsa.package_folder, include_dir)
            )
        for lib_dir in libalsa.cpp_info.libdirs:
            self.buildenv_info.append_path(
                "LIBRARY_PATH", os.path.join(libalsa.package_folder, lib_dir)
            )
