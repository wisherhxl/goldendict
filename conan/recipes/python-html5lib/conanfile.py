import os

from conan import ConanFile
from conan.tools.files import copy, get


class PythonHtml5LibRecipe(ConanFile):
    name = "python-html5lib"
    version = "1.1"
    package_type = "application"
    license = ("MIT", "BSD-3-Clause")
    homepage = "https://github.com/html5lib/html5lib-python"

    def source(self):
        for dependency in ("html5lib", "six", "webencodings"):
            source = self.conan_data["sources"][dependency]
            get(
                self,
                url=source["url"],
                sha256=source["sha256"],
                destination=dependency,
                strip_root=True,
            )

    def package(self):
        python_path = os.path.join(self.package_folder, "python")
        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "html5lib", "html5lib"),
            dst=os.path.join(python_path, "html5lib"),
        )
        copy(
            self,
            "six.py",
            src=os.path.join(self.source_folder, "six"),
            dst=python_path,
        )
        copy(
            self,
            "*",
            src=os.path.join(self.source_folder, "webencodings", "webencodings"),
            dst=os.path.join(python_path, "webencodings"),
        )
        for dependency in ("html5lib", "six", "webencodings"):
            copy(
                self,
                "LICENSE*",
                src=os.path.join(self.source_folder, dependency),
                dst=os.path.join(self.package_folder, "licenses", dependency),
            )

    def package_info(self):
        self.buildenv_info.prepend_path(
            "PYTHONPATH", os.path.join(self.package_folder, "python")
        )
