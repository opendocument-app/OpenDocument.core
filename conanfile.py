from conan import ConanFile
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake


class OpenDocumentCoreConan(ConanFile):
    name = "odrcore"
    version = ""
    url = "https://github.com/opendocument-app/OpenDocument.core"
    homepage = "https://opendocument.app/"
    description = "C++ library that translates office documents to HTML"
    topics = "open document", "openoffice xml", "open document reader"
    license = "MPL-2.0"

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        # removed and inert, kept only so a consumer still passing it does not
        # hard-fail on an unknown option; see CMakeLists.txt
        "with_libmagic": [True, False],
        "with_http_server": [True, False],
        "with_cli": [True, False],
        "with_python": [True, False],
        "with_jni": [True, False],
        "with_apple": [True, False],
        "with_wasm": [True, False],
        "bundle_assets": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_libmagic": False,
        "with_http_server": True,
        "with_cli": True,
        "with_python": False,
        "with_jni": False,
        "with_apple": False,
        "with_wasm": False,
        "bundle_assets": False,
        # paired with PUGIXML_COMPACT in CMakeLists.txt: no prebuilt library
        # to mismatch against the node layout the define changes
        "pugixml/*:header_only": True,
    }

    exports_sources = ["apple/*", "cli/*", "cmake/*", "jni/*", "python/*", "resources/dist/*", "wasm/*", "src/*", "CMakeLists.txt"]

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC
            del self.options.with_libmagic

    def requirements(self):
        self.requires("pugixml/1.15")
        self.requires("cryptopp/8.9.0")
        self.requires("md4c/0.5.2")
        self.requires("miniz/3.1.1")
        self.requires("nlohmann_json/3.12.0")
        self.requires("openjpeg/2.5.4")
        self.requires("uchardet/0.0.8")
        self.requires("utfcpp/4.0.9")
        if self.options.get_safe("with_http_server", False):
            self.requires("cpp-httplib/0.47.0")
        if self.options.get_safe("with_python", False):
            self.requires("pybind11/2.13.6")

    def build_requirements(self):
        self.test_requires("gtest/1.14.0")

    def validate_build(self):
        if self.settings.get_safe("compiler.cppstd"):
            check_min_cppstd(self, 20)

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_PROJECT_VERSION"] = self.version
        tc.variables["ODR_TEST"] = False
        # forwarded only so the CMake deprecation warning reaches a consumer
        # who still sets it; neither end does anything with it
        tc.variables["ODR_WITH_LIBMAGIC"] = self.options.get_safe("with_libmagic", False)
        tc.variables["ODR_WITH_HTTP_SERVER"] = self.options.get_safe("with_http_server", False)
        tc.variables["ODR_CLI"] = self.options.get_safe("with_cli", True)
        tc.variables["ODR_PYTHON"] = self.options.get_safe("with_python", False)
        tc.variables["ODR_JNI"] = self.options.get_safe("with_jni", False)
        tc.variables["ODR_APPLE"] = self.options.get_safe("with_apple", False)
        tc.variables["ODR_WASM"] = self.options.get_safe("with_wasm", False)
        tc.variables["ODR_BUNDLE_ASSETS"] = self.options.get_safe("bundle_assets", False)

        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["odr"]
        # the installed internal headers expose pugixml types, whose layout
        # PUGIXML_COMPACT changes
        self.cpp_info.defines = ["PUGIXML_COMPACT"]
