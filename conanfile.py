from conans import ConanFile, CMake


class IRKConan(ConanFile):
    name = "irkit"
    version = "0.1"
    url = "https://github.com/elshize/irkit"
    license = "MIT"
    description = "Information Retrieval tools intended for academic research."
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "ycm", "cmake_paths"
    options = {"use_system_boost": [True, False]}
    default_options = ("use_system_boost=False",
                       "zlib:shared=True",
                       "boost:without_math=True",
                       "boost:without_wave=True",
                       "boost:without_container=True",
                       "boost:without_exception=True",
                       "boost:without_graph=True",
                       "boost:without_locale=True",
                       "boost:without_log=True",
                       "boost:without_program_options=True",
                       "boost:without_random=True",
                       "boost:without_mpi=True",
                       "boost:without_serialization=True",
                       "boost:without_signals=True",
                       "boost:without_coroutine=True",
                       "boost:without_fiber=True",
                       "boost:without_timer=True",
                       "boost:without_graph_parallel=True",
                       "boost:without_python=True",
                       "boost:without_stacktrace=True",
                       "boost:without_system=True",
                       "boost:without_test=True",
                       "boost:without_thread=True",
                       "boost:without_type_erasure=True")
    exports_sources = ("LICENSE", "README.md", "include/*", "src/*",
                       "cmake/*", "CMakeLists.txt", "tests/*", "benchmarks/*",
                       "scripts/*")

    def build(self):
        cmake = CMake(self)
        if self.settings.compiler == 'gcc':
            cmake.definitions["CMAKE_C_COMPILER"] = "gcc-{}".format(
                self.settings.compiler.version)
            cmake.definitions["CMAKE_CXX_COMPILER"] = "g++-{}".format(
                self.settings.compiler.version)
        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = 'conan_paths.cmake'
        cmake.configure()
        cmake.build()
        cmake.test()

    def requirements(self):
        self.requires("streamvbyte/master@elshize/testing")
        self.requires("gumbo-parser/1.0@elshize/stable")
        self.requires("rax/master@elshize/testing")
        self.requires("irm/0.1@elshize/develop")
        self.requires("cppitertools/1.0@elshize/stable")
        self.requires("taily/0.1@elshize/testing")
        self.requires("ParallelSTL/20181004@elshize/stable")

        if not self.options.use_system_boost:
            self.requires("boost/1.66.0@conan/stable")
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("gtest/1.8.0@conan/stable")
        self.requires("TBB/2018_U5@conan/stable")

        self.requires("range-v3/0.4.0@ericniebler/stable")
        self.requires("CLI11/1.6.0@cliutils/stable")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")
        self.requires("debug_assert/1.3@Manu343726/testing")
        self.requires("type_safe/0.1@Manu343726/testing")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")
        self.requires("jsonformoderncpp/3.1.2@vthiery/stable")
        self.requires("spdlog/1.1.0@bincrafters/stable")

    def configure(self):
        self.options["boost"].shared = False

    def package(self):
        cmake = CMake(self)
        cmake.install()
