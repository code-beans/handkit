import os
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, get


class HandkitSDL(ConanFile):
    name = "handkit-sdl"
    version = "3.2.10"
    package_type = "static-library"
    settings = "os", "arch", "compiler", "build_type"
    options = {"glibc_version": ["2.33"]}
    default_options = {"glibc_version": "2.33"}
    description = "SDL3 with the Handkit fbdev EGL/Vulkan backend (PowerVR, Mali)"
    license = "Zlib", "MIT"  # SDL is Zlib; the Handkit backend is MIT.
    exports_sources = "CMakeLists.txt"

    def export_sources(self):
        copy(self, "sdl_handkit_*", os.path.join(self.recipe_folder, "..", "..", "src", "platform"), os.path.join(self.export_sources_folder, "backend"))

    def configure(self):
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def validate(self):
        if self.settings.os != "Linux" or self.settings.arch != "armv8":
            raise ConanInvalidConfiguration("This package targets ARM64 Linux handhelds")
        if self.settings.compiler != "clang" or str(self.settings.compiler.version) != "19":
            raise ConanInvalidConfiguration("Use profiles/aarch64 (Zig 0.14.1 / Clang 19)")

    def layout(self):
        cmake_layout(self)

    def build_requirements(self):
        self.tool_requires("handkit-toolchain/0.14.1")

    def source(self):
        get(self, **self.conan_data["sources"][str(self.version)], destination=os.path.join(self.source_folder, "sdl"), strip_root=True)

    def generate(self):
        CMakeToolchain(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install()
        copy(self, "LICENSE.txt", os.path.join(self.source_folder, "sdl"), os.path.join(self.package_folder, "licenses", "sdl"))
        # SDL's public GLES headers and Vulkan diagnostics need Khronos headers.
        copy(self, "*", os.path.join(self.source_folder, "sdl", "src", "video", "khronos"), os.path.join(self.package_folder, "include"))

    def package_info(self):
        self.cpp_info.libs = ["SDL3"]
        self.cpp_info.system_libs = ["m", "dl", "pthread", "rt"]
        self.cpp_info.set_property("cmake_file_name", "SDL3")
        self.cpp_info.set_property("cmake_target_name", "SDL3::SDL3")
