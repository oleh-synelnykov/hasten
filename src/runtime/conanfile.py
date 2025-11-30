from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps

class HastenRuntimeConan(ConanFile):
    name = "hasten_runtime"
    version = "2.0.0"
    license = "Apache-2.0"
    description = "Runtime support library for Hasten-generated C++ bindings."
    url = "https://github.com/oleh-synelnykov/hasten"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "CMakeLists.txt", "include/*", "*.cpp"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["hasten_runtime"]
