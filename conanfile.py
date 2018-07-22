from conans import ConanFile, CMake

#boost_libs = ['math', 'wave', 'container', 'contract', 'exception', 'graph', 'iostreams', 'locale', 'log',
#              'program_options', 'random', 'regex', 'mpi', 'serialization', 'signals',
#              'coroutine', 'fiber', 'context', 'timer', 'thread', 'chrono', 'date_time',
#              'atomic', 'filesystem', 'system', 'graph_parallel', 'python',
#              'stacktrace', 'test', 'type_erasure']

#included_libs = ['filesystem', 'system', 'iostreams', 'log', 'thread']


class HelloConan(ConanFile):
    name = "irkit"
    version = "0.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"
    default_options = ("zlib:shared=True",
                       "boost:without_math=True",
                       "boost:without_wave=True",
                       "boost:without_container=True",
                       "boost:without_exception=True",
                       "boost:without_graph=True",
                       "boost:without_locale=True",
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
                       "boost:without_test=True",
                       "boost:without_type_erasure=True")

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
        self.requires("rax/master@elshize/testing")

        self.requires("boost/1.66.0@conan/stable")
        self.requires("zlib/1.2.11@conan/stable")
        self.requires("gtest/1.8.0@conan/stable")

        self.requires("CLI11/1.6.0@cliutils/stable")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")
        self.requires("debug_assert/1.3@Manu343726/testing")
        self.requires("type_safe/0.1@Manu343726/testing")
        self.requires("gsl_microsoft/1.0.0@bincrafters/stable")
        self.requires("jsonformoderncpp/3.1.2@vthiery/stable")

    def configure(self):
        self.options["boost"].shared = True
