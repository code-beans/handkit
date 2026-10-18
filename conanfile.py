from conan import ConanFile
from conan.tools.cmake import CMakeDeps, CMakeToolchain, cmake_layout


class HandkitExamples(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    default_options = {"glslang/*:enable_optimizer": False, "glslang/*:build_executables": True}

    def requirements(self):
        self.requires("handkit/0.4.0")

    def build_requirements(self):
        self.tool_requires("handkit-toolchain/0.14.1")
        self.tool_requires("glslang/1.4.357.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        CMakeDeps(self).generate()
        toolchain = CMakeToolchain(self)
        toolchain.variables["HANDKIT"] = True
        toolchain.generate()
