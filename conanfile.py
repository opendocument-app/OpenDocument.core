from conans import ConanFile, CMake


class OpenDocumentCoreConan(ConanFile):
    name = "odrcore"
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

    exports_sources = ["cli/*", "cmake/*", "src/*", "CMakeLists.txt"]

    requires = ["pugixml/1.14", "cryptopp/8.8.0", "miniz/2.2.0", "nlohmann_json/3.11.3",
                "vincentlaucsb-csv-parser/2.1.3", "uchardet/0.0.7"]
    build_requires = ["gtest/1.14.0"]
    generators = "cmake_paths", "cmake_find_package"

    _cmake = None

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = CMake(self)
        self._cmake.definitions["CMAKE_PROJECT_VERSION"] = self.version
        self._cmake.definitions["BUILD_SHARED_LIBS"] = self.options.shared
        self._cmake.definitions["ODR_TEST"] = False
        self._cmake.configure()
        return self._cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        self.copy("*.hpp", src="src", dst="include")

        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["odr"]

        self.cpp_info.names["cmake_find_package"] = "odr"
        self.cpp_info.names["cmake_find_package_multi"] = "odr"
        self.cpp_info.names["pkgconfig"] = "libodr"
