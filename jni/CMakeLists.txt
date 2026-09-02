# JNI bindings for OpenDocument.core (Java package `app.opendocument.core`).
#
# Included from the top-level CMakeLists.txt when `ODR_JNI` is ON. Can also be
# configured standalone against an installed `odrcore` package.

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    project(odrjni LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    find_package(odrcore REQUIRED)
    set(ODR_JNI_ODR_TARGET odrcore::odrcore)

    # Same option name as the root build; an installed odrcore ships
    # `http_server.hpp` only when it was built with the HTTP server.
    option(ODR_WITH_HTTP_SERVER "Build with cpp-httplib HTTP server" ON)
else ()
    set(ODR_JNI_ODR_TARGET odr)
endif ()

# On android the NDK sysroot ships `jni.h` and the JNI symbols come from the
# runtime, so there is nothing to find or link against.
if (NOT ANDROID)
    find_package(JNI REQUIRED)
endif ()

# Android is no reason to skip the jar — the android odrcore package ships it
# next to `libodr_jni.so` for OpenDocument.droid. Only the AAR build wants the
# native half alone, and asks for it, so a missing JDK fails rather than
# quietly shipping half a package.
option(ODR_JNI_JAR "Build the java half, `odr-core-java.jar`" ON)
if (ODR_JNI_JAR)
    find_package(Java 11 REQUIRED COMPONENTS Development)
    include(UseJava)
endif ()

add_library(odr_jni SHARED
        "src/odr_jni.cpp"
        "src/jni_core.cpp"
        "src/jni_document.cpp"
        "src/jni_file.cpp"
        "src/jni_html.cpp"
        "src/jni_logger.cpp"
        "src/jni_http_server.cpp"
        "src/jni_style.cpp"
)
if (NOT ANDROID)
    target_include_directories(odr_jni PRIVATE ${JNI_INCLUDE_DIRS})
endif ()
target_link_libraries(odr_jni PRIVATE ${ODR_JNI_ODR_TARGET})
if (ODR_WITH_HTTP_SERVER)
    target_compile_definitions(odr_jni PRIVATE ODR_WITH_HTTP_SERVER)
endif ()

install(TARGETS odr_jni LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT jni)

if (NOT ODR_JNI_JAR)
    return()
endif ()

set(CMAKE_JAVA_COMPILE_FLAGS --release 17 -Xlint:all)

add_jar(odr_java
        SOURCES
        "java/app/opendocument/core/AnchorType.java"
        "java/app/opendocument/core/Archive.java"
        "java/app/opendocument/core/ArchiveFile.java"
        "java/app/opendocument/core/Bookmark.java"
        "java/app/opendocument/core/BreakType.java"
        "java/app/opendocument/core/Circle.java"
        "java/app/opendocument/core/Color.java"
        "java/app/opendocument/core/CustomShape.java"
        "java/app/opendocument/core/DecodePreference.java"
        "java/app/opendocument/core/DecodedFile.java"
        "java/app/opendocument/core/DirectionalMeasure.java"
        "java/app/opendocument/core/DirectionalString.java"
        "java/app/opendocument/core/Document.java"
        "java/app/opendocument/core/DocumentFile.java"
        "java/app/opendocument/core/DocumentPath.java"
        "java/app/opendocument/core/DocumentType.java"
        "java/app/opendocument/core/DrawingPath.java"
        "java/app/opendocument/core/DrawingTransform.java"
        "java/app/opendocument/core/Element.java"
        "java/app/opendocument/core/ElementType.java"
        "java/app/opendocument/core/EncryptionState.java"
        "java/app/opendocument/core/File.java"
        "java/app/opendocument/core/FileCategory.java"
        "java/app/opendocument/core/FileLocation.java"
        "java/app/opendocument/core/FileMeta.java"
        "java/app/opendocument/core/FileType.java"
        "java/app/opendocument/core/FileTypeCapabilities.java"
        "java/app/opendocument/core/FileWalker.java"
        "java/app/opendocument/core/Filesystem.java"
        "java/app/opendocument/core/FontFile.java"
        "java/app/opendocument/core/FontPosition.java"
        "java/app/opendocument/core/FontStyle.java"
        "java/app/opendocument/core/FontWeight.java"
        "java/app/opendocument/core/ILogger.java"
        "java/app/opendocument/core/LogLevel.java"
        "java/app/opendocument/core/Logger.java"
        "java/app/opendocument/core/LoggerBridge.java"
        "java/app/opendocument/core/SourceLocation.java"
        "java/app/opendocument/core/Frame.java"
        "java/app/opendocument/core/GlobalParams.java"
        "java/app/opendocument/core/GraphicStyle.java"
        "java/app/opendocument/core/GuardedNativeResource.java"
        "java/app/opendocument/core/HorizontalAlign.java"
        "java/app/opendocument/core/Html.java"
        "java/app/opendocument/core/HtmlColorScheme.java"
        "java/app/opendocument/core/HtmlConfig.java"
        "java/app/opendocument/core/HtmlPage.java"
        "java/app/opendocument/core/HtmlResource.java"
        "java/app/opendocument/core/HtmlResourceType.java"
        "java/app/opendocument/core/HtmlService.java"
        "java/app/opendocument/core/HtmlSheetCut.java"
        "java/app/opendocument/core/HtmlTableGridlines.java"
        "java/app/opendocument/core/HtmlView.java"
        "java/app/opendocument/core/HtmlViewportMode.java"
        "java/app/opendocument/core/HttpServer.java"
        "java/app/opendocument/core/Image.java"
        "java/app/opendocument/core/ImageFile.java"
        "java/app/opendocument/core/Line.java"
        "java/app/opendocument/core/LineBreak.java"
        "java/app/opendocument/core/Link.java"
        "java/app/opendocument/core/ListElement.java"
        "java/app/opendocument/core/ListItem.java"
        "java/app/opendocument/core/ListType.java"
        "java/app/opendocument/core/MasterPage.java"
        "java/app/opendocument/core/Measure.java"
        "java/app/opendocument/core/NativeLibrary.java"
        "java/app/opendocument/core/NativeResource.java"
        "java/app/opendocument/core/Odr.java"
        "java/app/opendocument/core/OdrException.java"
        "java/app/opendocument/core/Page.java"
        "java/app/opendocument/core/PageLayout.java"
        "java/app/opendocument/core/Paragraph.java"
        "java/app/opendocument/core/ParagraphStyle.java"
        "java/app/opendocument/core/PdfFile.java"
        "java/app/opendocument/core/PdfTextMode.java"
        "java/app/opendocument/core/PrintOrientation.java"
        "java/app/opendocument/core/Rect.java"
        "java/app/opendocument/core/Sheet.java"
        "java/app/opendocument/core/SheetCell.java"
        "java/app/opendocument/core/Slide.java"
        "java/app/opendocument/core/Span.java"
        "java/app/opendocument/core/Table.java"
        "java/app/opendocument/core/TableCell.java"
        "java/app/opendocument/core/TableCellStyle.java"
        "java/app/opendocument/core/TableColumn.java"
        "java/app/opendocument/core/TableColumnStyle.java"
        "java/app/opendocument/core/TableDimensions.java"
        "java/app/opendocument/core/TablePosition.java"
        "java/app/opendocument/core/TableRow.java"
        "java/app/opendocument/core/TableRowStyle.java"
        "java/app/opendocument/core/TableStyle.java"
        "java/app/opendocument/core/Text.java"
        "java/app/opendocument/core/TextAlign.java"
        "java/app/opendocument/core/TextFile.java"
        "java/app/opendocument/core/TextRoot.java"
        "java/app/opendocument/core/TextStyle.java"
        "java/app/opendocument/core/TextWrap.java"
        "java/app/opendocument/core/ValueType.java"
        "java/app/opendocument/core/VerticalAlign.java"
        OUTPUT_NAME odr-core-java
)

install_jar(odr_java DESTINATION "${CMAKE_INSTALL_DATADIR}/java" COMPONENT jni)

# The junit suite loads `libodr_jni` into the JVM that runs it, so it is only
# meaningful for a host build; the android build is covered by the instrumented
# tests of the AAR (`android/`).
if (ODR_TEST AND NOT ANDROID)
    # `add_jar`'s `RESOURCES` below. Say so here rather than let it fail on an
    # unknown argument; the rest of the JNI build is fine on the repo minimum.
    if (CMAKE_VERSION VERSION_LESS 3.21)
        message(FATAL_ERROR
                "The JNI test jar needs CMake 3.21 or newer, found ${CMAKE_VERSION}. "
                "Configure with -DODR_TEST=OFF to build the bindings without it.")
    endif ()

    include(FetchContent)
    FetchContent_Declare(junit_console
            URL "https://repo1.maven.org/maven2/org/junit/platform/junit-platform-console-standalone/1.10.2/junit-platform-console-standalone-1.10.2.jar"
            URL_HASH SHA256=a1de557821293ce903c213c694165fff532cf92081bac4238b9e05b35f04f43f
            DOWNLOAD_NO_EXTRACT TRUE
    )
    FetchContent_MakeAvailable(junit_console)
    set(ODR_JNI_JUNIT_JAR
            "${junit_console_SOURCE_DIR}/junit-platform-console-standalone-1.10.2.jar")

    add_jar(odr_java_tests
            SOURCES
            "tests/app/opendocument/core/DocumentTest.java"
            "tests/app/opendocument/core/FileTest.java"
            "tests/app/opendocument/core/HtmlTest.java"
            "tests/app/opendocument/core/HttpServerTest.java"
            "tests/app/opendocument/core/LoggerTest.java"
            "tests/app/opendocument/core/MetaTest.java"
            # shared with the instrumented suite of the AAR, see `android/`
            "testfixtures/app/opendocument/core/TestFiles.java"
            # `RESOURCES NAMESPACE` rather than listing the file under SOURCES:
            # the latter puts it in the jar at the path as written, and
            # `TestFiles` looks it up next to its own class.
            # No leading slash — the namespace goes into the `jar` command line
            # verbatim, where an absolute path is not found.
            RESOURCES NAMESPACE "app/opendocument/core"
            "testfixtures/resources/app/opendocument/core/mixed-layout.odt"
            INCLUDE_JARS odr_java "${ODR_JNI_JUNIT_JAR}"
            OUTPUT_NAME odr-core-java-tests
    )

    enable_testing()
    add_test(NAME odr_jni_junit
            COMMAND "${Java_JAVA_EXECUTABLE}"
            "-Djava.library.path=$<TARGET_FILE_DIR:odr_jni>"
            -jar "${ODR_JNI_JUNIT_JAR}"
            execute
            --class-path "$<TARGET_PROPERTY:odr_java,JAR_FILE>"
            --class-path "$<TARGET_PROPERTY:odr_java_tests,JAR_FILE>"
            --scan-class-path
            --fail-if-no-tests
    )
endif ()
