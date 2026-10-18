import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy


class Handkit(ConanFile):
    name = "handkit"
    version = "0.4.0"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    license = "MIT"
    description = "Handkit: SDL3 display integration and application helpers for Linux handhelds"

    def export_sources(self):
        copy(self, "*", os.path.join(self.recipe_folder, "..", "..", "sdk"), self.export_sources_folder)

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self)

    def requirements(self):
        self.requires("handkit-sdl/3.2.10", transitive_headers=True, transitive_libs=True)

    def build_requirements(self):
        self.tool_requires("handkit-toolchain/0.14.1")

    def generate(self):
        CMakeDeps(self).generate()
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()

    def package_info(self):
        self.cpp_info.libs = ["handkit"]
        self.cpp_info.system_libs = ["dl"]
        self.cpp_info.set_property("cmake_file_name", "Handkit")
        self.cpp_info.set_property("cmake_target_name", "Handkit::Handkit")
        self.cpp_info.set_property("cmake_build_modules", ["lib/cmake/Handkit/HandkitTools.cmake"])
