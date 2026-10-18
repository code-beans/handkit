import os
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.files import copy, get, save


class HandkitToolchain(ConanFile):
    name = "handkit-toolchain"
    version = "0.14.1"
    package_type = "application"
    settings = "os", "arch"
    license = "MIT"
    description = "Pinned Zig cross compiler targeting ARM64 Linux/glibc 2.33 handhelds"

    def validate(self):
        if f"{self.settings.os}-{self.settings.arch}" not in self.conan_data["sources"]:
            raise ConanInvalidConfiguration("Supported build machines: macOS/Linux, ARM64/x86_64")

    def build(self):
        get(self, **self.conan_data["sources"][f"{self.settings.os}-{self.settings.arch}"], destination=os.path.join(self.build_folder, "zig"), strip_root=True)

    def package(self):
        copy(self, "*", os.path.join(self.build_folder, "zig"), os.path.join(self.package_folder, "zig"))
        copy(self, "LICENSE", os.path.join(self.build_folder, "zig"), os.path.join(self.package_folder, "licenses"))
        for name, args in {"handkit-cc": "cc -target aarch64-linux-gnu.2.33", "handkit-cxx": "c++ -target aarch64-linux-gnu.2.33", "handkit-ar": "ar", "handkit-ranlib": "ranlib"}.items():
            path = os.path.join(self.package_folder, "bin", name)
            save(self, path, f'#!/bin/sh\nexec "$(dirname "$0")/../zig/zig" {args} "$@"\n')
            os.chmod(path, 0o755)
        save(
            self,
            os.path.join(self.package_folder, "cmake", "handkit.cmake"),
            """
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
get_filename_component(HANDKIT_TOOLCHAIN_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CMAKE_C_COMPILER "${HANDKIT_TOOLCHAIN_ROOT}/bin/handkit-cc")
set(CMAKE_CXX_COMPILER "${HANDKIT_TOOLCHAIN_ROOT}/bin/handkit-cxx")
set(CMAKE_AR "${HANDKIT_TOOLCHAIN_ROOT}/bin/handkit-ar" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${HANDKIT_TOOLCHAIN_ROOT}/bin/handkit-ranlib" CACHE FILEPATH "" FORCE)
""",
        )

    def package_info(self):
        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.conf_info.append("tools.cmake.cmaketoolchain:user_toolchain", os.path.join(self.package_folder, "cmake", "handkit.cmake"))
