from conans import ConanFile, CMake


class OpenDocumentCoreConan(ConanFile):
    settings = "os", "compiler", "cppstd", "build_type", "arch"
    requires = "pugixml/1.11", "cryptopp/8.5.0", "miniz/2.1.0", "nlohmann_json/3.10.2",\
               "vincentlaucsb-csv-parser/2.1.3", "gtest/1.11.0", "glog/0.4.0"
    generators = "cmake", "gcc", "txt"
    default_options = {}

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
