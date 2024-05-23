import os
from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.files import copy
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake


class OpenDocumentCoreConan(ConanFile):
    name = "odrcore"
    version = ""
    url = ""
    homepage = "https://github.com/opendocument-app/OpenDocument.core"
    description = "C++ library that translates office documents to HTML"
    topics = "open document", "openoffice xml", "open document reader"
    license = "GPL 3.0"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = ["cli/*", "cmake/*", "src/*", "CMakeLists.txt"]

    def requirements(self):
        self.requires("pugixml/1.14")
        self.requires("cryptopp/8.8.0")
        self.requires("miniz/3.0.2")
        self.requires("nlohmann_json/3.11.3")
        self.requires("vincentlaucsb-csv-parser/2.1.3")
        self.requires("uchardet/0.0.7")
        self.requires("utfcpp/4.0.4")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def validate_build(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, 17)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_PROJECT_VERSION"] = self.version
        tc.variables["BUILD_SHARED_LIBS"] = self.options.shared
        tc.variables["ODR_TEST"] = False
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(
            self,
            "*.hpp",
            src=os.path.join(self.recipe_folder, "src"),
            dst=os.path.join(self.export_sources_folder, "include"),
        )

        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["odr"]
