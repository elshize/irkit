from conans import ConanFile, CMake


class HelloConan(ConanFile):
    name = "irkit"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"

    def source(self):
        self.run("git clone https://github.com/elshize/irkit.git")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()

    def requirements(self):
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("gumbo-parser/1.0@elshize/stable")
