from conans import ConanFile, CMake


class HelloConan(ConanFile):
    name = "irkit"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    default_options = "zlib:shared=True"

    def source(self):
        self.run("git clone https://github.com/elshize/irkit.git")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()

    def requirements(self):
        self.requires("streamvbyte/master@elshize/testing")
        self.requires("gumbo-parser/1.0@elshize/stable")
        self.requires("rangev3/master@elshize/testing")
        self.requires("nlohmann_json/3.1.2@elshize/stable")
        self.requires("rax/master@elshize/testing")

        self.requires("boost/1.67.0@conan/stable")
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("gtest/1.8.0@conan/stable")

        self.requires("CLI11/1.6.0@cliutils/stable")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")
        self.requires("debug_assert/1.3@Manu343726/testing")
        self.requires("type_safe/0.1@Manu343726/testing")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")

    def configure(self):
        self.options["boost"].shared = True
