from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps
from conan.tools.files import copy
import os


class HastenRuntimeConan(ConanFile):
    name = "hasten_runtime"
    version = "2.0.0"
    license = "Apache-2.0"
    description = "Runtime support library for Hasten-generated C++ bindings."
    url = "https://github.com/oleh-synelnykov/hasten"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "CMakeLists.txt", "include/*"
    # Header-only
    no_copy_source = True
    package_type = "header-library"

    def package(self):
        copy(self, "*.hpp", self.source_folder, self.package_folder)
        copy(self, "LICENSE", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)

    def package_info(self):
        # For header-only packages, libdirs and bindirs are not used
        # so it's necessary to set those as empty.
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
