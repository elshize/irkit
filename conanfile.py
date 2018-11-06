from conans import ConanFile, CMake


class IRKConan(ConanFile):
    name = "irkit"
    version = "0.1"
    url = "https://github.com/elshize/irkit"
    license = "MIT"
    description = "Information Retrieval tools intended for academic research."
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "ycm", "cmake_paths"
    options = {"use_system_boost": [True, False], "no_sanitizers": [True, False]}
    default_options = ("use_system_boost=False",
                       "no_sanitizers=False")
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
        if self.options.no_sanitizers:
            cmake.definitions["IRKit_NO_SANITIZERS"] = 'ON'
        cmake.definitions["CMAKE_TOOLCHAIN_FILE"] = 'conan_paths.cmake'
        cmake.configure()
        cmake.build()
        cmake.test()

    def requirements(self):
        self.requires("streamvbyte/master@elshize/testing")
        self.requires("gumbo-parser/1.0@elshize/stable")
        self.requires("rax/master@elshize/testing")
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
        self.requires("catch2/2.4.1@bincrafters/stable")

    def configure(self):
        self.options["taily"].use_system_boost = self.options.use_system_boost

    def package(self):
        cmake = CMake(self)
        cmake.install()
