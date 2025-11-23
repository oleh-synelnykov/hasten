from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class hastenRecipe(ConanFile):
    name = "hasten"
    version = "2.0.0"
    package_type = "application"

    # Optional metadata
    license = "Apache-2.0"
    author = "Oleh Synelnykov"
    url = "https://github.com/oleh-synelnykov/hasten"
    description = "Local RPC system for C++"
    topics = ("rpc", "ipc", "idl")

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    def requirements(self):
        self.requires("boost/1.89.0")
        self.requires("spdlog/1.16.0")
        self.requires("gtest/1.17.0")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.preprocessor_definitions["HASTEN_VERSION_MAJOR"] = "2"
        tc.preprocessor_definitions["HASTEN_VERSION_MINOR"] = "0"
        tc.preprocessor_definitions["HASTEN_VERSION_PATCH"] = "0"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
