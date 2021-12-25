from conans import ConanFile, CMake


class OpenDocumentCoreConan(ConanFile):
    name = "odr.core"
    url = ""
    homepage = "https://github.com/opendocument-app/OpenDocument.core"
    description = "C++ library that translates office documents to HTML"
    topics = "open document", "openoffice xml", "open document reader"
    license = "GPL 3.0"

    settings = "os", "compiler", "cppstd", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    requires = ["pugixml/1.11", "cryptopp/8.5.0", "miniz/2.1.0", "nlohmann_json/3.10.4",
                "vincentlaucsb-csv-parser/2.1.3", "uchardet/0.0.7", "gtest/1.11.0"]
    generators = "cmake_paths", "cmake_find_package"

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
