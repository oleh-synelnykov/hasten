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
        boost_opts = {
            'without_atomic': True,
            'without_charconv': True,
            'without_chrono': True,
            'without_cobalt': True,
            'without_container': True,
            'without_context': True,
            'without_contract': True,
            'without_coroutine': True,
            'without_date_time': True,
            'without_exception': True,
            'without_fiber': True,
            'without_filesystem': True,
            'without_graph': True,
            'without_graph_parallel': True,
            'without_iostreams': True,
            'without_json': True,
            'without_locale': True,
            'without_log': True,
            'without_math': True,
            'without_mpi': True,
            'without_nowide': True,
            'without_process': True,
            'without_program_options': False,
            'without_python': True,
            'without_random': True,
            'without_regex': True,
            'without_serialization': True,
            'without_stacktrace': True,
            'without_test': True,
            'without_thread': True,
            'without_timer': True,
            'without_type_erasure': True,
            'without_url': True,
            'without_wave': True,
        }
        self.requires("boost/1.89.0", options=boost_opts)
        self.requires("spdlog/1.16.0")
        self.requires("nlohmann_json/3.12.0")
        self.test_requires("gtest/1.17.0")

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
