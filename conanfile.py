from conan import ConanFile
from conan.tools import check_min_cppstd
from conan.tools.files import copy
from conan.tools.scm import Git
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout
from conan.tools.files import update_conandata


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

    generators = "CMakeToolchain", "CMakeDeps"

    exports_sources = ["cli/*", "cmake/*", "src/*", "CMakeLists.txt"]

    def requirements(self):
        self.requires("pugixml/1.14")
        self.requires("cryptopp/8.8.0")
        self.requires("miniz/3.0.2")
        self.requires("nlohmann_json/3.11.3")
        self.requires("vincentlaucsb-csv-parser/2.1.3")
        self.requires("uchardet/0.0.7")
        self.requires("utfcpp/4.0.4")

        self.test_requires("gtest/1.14.0")

    def validate(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, 17)

    def layout(self):
        cmake_layout(self)

    def export(self):
        git = Git(self, self.recipe_folder)
        scm_url, scm_commit = git.get_url_and_commit()
        update_conandata(self, {"sources": {"commit": scm_commit, "url": scm_url}})

    def source(self):
        git = Git(self)
        sources = self.conan_data["sources"]
        git.clone(url=sources["url"], target=".")
        git.checkout(commit=sources["commit"])

    def configure(self):
        pass

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
        copy(self, "*.hpp", src=self.recipe_folder / "src", dst=self.export_sources_folder / "include")

        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["odr"]

        self.cpp_info.names["cmake_find_package"] = "odr"
        self.cpp_info.names["cmake_find_package_multi"] = "odr"
        self.cpp_info.names["pkgconfig"] = "libodr"
