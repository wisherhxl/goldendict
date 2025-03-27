from conan import ConanFile
from conan.tools.cmake import cmake_layout


class CompressorRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        # self.requires("zlib/[>1.3]")
        self.requires("protobuf/[>3.0]")
        # self.requires(
        #     "qt/[>5.15]",
        #     options={"qttranslations": True, "qtserialport": True}
        # )
        # self.requires("log4cplus/[>2.0]")

    def build_requirements(self):
        self.tool_requires("cmake/[<3.27]")

    def layout(self):
        cmake_layout(self)
