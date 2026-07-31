# The Objective-C bindings, built as a dynamic framework — the slice that
# `build_xcframework.py` assembles into `OdrCoreObjC.xcframework`.
#
# Dynamic and not static, because `+load` (see `src/OdrCoreBootstrap.mm`) is what
# points odrcore at the resources in this bundle, and a static archive member
# nothing references is dropped by the linker. SwiftPM binary targets give a
# consumer no way to pass `-ObjC`/`-force_load`, so there is no recovering
# from that on their side.

if (NOT APPLE)
    message(FATAL_ERROR "ODR_APPLE needs an Apple toolchain")
endif ()
if (BUILD_SHARED_LIBS)
    message(FATAL_ERROR
            "ODR_APPLE needs BUILD_SHARED_LIBS=OFF: the framework is the only "
            "dylib, odrcore is linked into it")
endif ()
if (NOT ODR_BUNDLE_ASSETS)
    message(FATAL_ERROR
            "ODR_APPLE needs ODR_BUNDLE_ASSETS=ON: the renderer's css/js and "
            "the libmagic database ship as bundle resources")
endif ()

enable_language(OBJCXX)

# `CMAKE_OSX_SYSROOT` is the only thing that distinguishes the three slices, and
# the Info.plist needs all of it spelled out.
if (CMAKE_OSX_SYSROOT MATCHES "iPhoneOS")
    set(ODR_APPLE_PLATFORM "iPhoneOS")
    set(ODR_APPLE_SDK_NAME "iphoneos")
elseif (CMAKE_OSX_SYSROOT MATCHES "iPhoneSimulator")
    set(ODR_APPLE_PLATFORM "iPhoneSimulator")
    set(ODR_APPLE_SDK_NAME "iphonesimulator")
else ()
    set(ODR_APPLE_PLATFORM "MacOSX")
    set(ODR_APPLE_SDK_NAME "macosx")
endif ()
set(ODR_APPLE_FRAMEWORK_VERSION "A")
set(ODR_APPLE_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}")
if (NOT ODR_APPLE_DEPLOYMENT_TARGET)
    message(FATAL_ERROR
            "CMAKE_OSX_DEPLOYMENT_TARGET is empty. Pin `os.version` in the "
            "conan profile — an unset deployment target floats with the SDK "
            "and would disagree with Package.swift's `platforms:`.")
endif ()

set(ODR_APPLE_PUBLIC_HEADERS
        "include/OdrCoreObjC/OdrCoreObjC.h"
        "include/OdrCoreObjC/ODRError.h"
        "include/OdrCoreObjC/ODRGlobalParams.h"
        "include/OdrCoreObjC/ODRDocument.h"
        "include/OdrCoreObjC/ODRFile.h"
        "include/OdrCoreObjC/ODRHtml.h"
        "include/OdrCoreObjC/ODRLogger.h"
        "include/OdrCoreObjC/ODROdr.h"
        "include/OdrCoreObjC/ODRTable.h"
)

# The renderer data the root build already staged, plus the libmagic database
# `ODR_BUNDLE_ASSETS` copied in beside it.
file(GLOB ODR_APPLE_RESOURCES "${ODR_BUILD_ODR_DATA_PATH}/*")
if (NOT ODR_APPLE_RESOURCES)
    message(FATAL_ERROR "no resources in ${ODR_BUILD_ODR_DATA_PATH}")
endif ()

add_library(odr_apple SHARED
        "src/ODRInternal.mm"
        "src/OdrCoreBootstrap.mm"
        "src/ODRGlobalParams.mm"
        "src/ODRDocument.mm"
        "src/ODRFile.mm"
        "src/ODRHtml.mm"
        "src/ODRLogger.mm"
        "src/ODROdr.mm"
        "src/ODRTable.mm"
)
target_include_directories(odr_apple PRIVATE "include" "src")
target_link_libraries(odr_apple PRIVATE odr "-framework Foundation")
target_compile_options(odr_apple PRIVATE -fobjc-arc)

configure_file("Info.plist.in" "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)

set_target_properties(odr_apple PROPERTIES
        OUTPUT_NAME "OdrCoreObjC"
        FRAMEWORK TRUE
        FRAMEWORK_VERSION "${ODR_APPLE_FRAMEWORK_VERSION}"
        MACOSX_FRAMEWORK_IDENTIFIER "app.opendocument.OdrCoreObjC"
        MACOSX_FRAMEWORK_BUNDLE_VERSION "${CMAKE_PROJECT_VERSION}"
        MACOSX_FRAMEWORK_SHORT_VERSION_STRING "${CMAKE_PROJECT_VERSION}"
        MACOSX_FRAMEWORK_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
        # the packaging copies the build output verbatim, so the built and the
        # shipped install name have to be the same
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR TRUE
)

# Export list rather than visibility flags — see the file's own comment.
set(ODR_APPLE_EXPORTS "${CMAKE_CURRENT_SOURCE_DIR}/exported_symbols.txt")
target_link_options(odr_apple PRIVATE
        "LINKER:-exported_symbols_list,${ODR_APPLE_EXPORTS}"
        "LINKER:-dead_strip"
)
set_property(TARGET odr_apple APPEND PROPERTY LINK_DEPENDS "${ODR_APPLE_EXPORTS}")

# Headers, Resources and Modules are staged by hand.
#
# `PUBLIC_HEADER` and `RESOURCE` are the obvious way to do the first two, and
# on CMake 3.28 with the Ninja generator they copy nothing at all — the
# framework comes out with an empty `Resources` and no `Headers`, silently. A
# framework that builds and then cannot be imported, with the renderer's assets
# missing at runtime, is not a failure mode worth risking on a generator quirk.
# `Modules/` has no property to begin with. So all three are explicit.
#
# The content directory is spelled out rather than taken from
# `TARGET_BUNDLE_CONTENT_DIR`, which expands to the bundle root even on macOS,
# where the content actually lives in `Versions/<v>`.
if (CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
    set(ODR_APPLE_CONTENT "$<TARGET_BUNDLE_DIR:odr_apple>")
else ()
    set(ODR_APPLE_CONTENT
            "$<TARGET_BUNDLE_DIR:odr_apple>/Versions/${ODR_APPLE_FRAMEWORK_VERSION}")
endif ()

add_custom_command(TARGET odr_apple POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
        "${ODR_APPLE_CONTENT}/Headers" "${ODR_APPLE_CONTENT}/Modules"
        "${ODR_APPLE_CONTENT}/Resources"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${ODR_APPLE_PUBLIC_HEADERS} "${ODR_APPLE_CONTENT}/Headers"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap"
        "${ODR_APPLE_CONTENT}/Modules"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        ${ODR_APPLE_RESOURCES} "${ODR_APPLE_CONTENT}/Resources"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Staging OdrCoreObjC.framework headers, module map and resources"
        VERBATIM
)

if (NOT CMAKE_SYSTEM_NAME MATCHES "^(iOS|tvOS|watchOS|visionOS)$")
    # The versioned layout wants the three of them reachable from the bundle
    # root too. `rm -rf` first, so a rebuild can replace what the previous one
    # left behind.
    foreach (directory IN ITEMS "Headers" "Modules" "Resources")
        add_custom_command(TARGET odr_apple POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E rm -rf
                "$<TARGET_BUNDLE_DIR:odr_apple>/${directory}"
                COMMAND "${CMAKE_COMMAND}" -E create_symlink
                "Versions/Current/${directory}"
                "$<TARGET_BUNDLE_DIR:odr_apple>/${directory}"
                VERBATIM
        )
    endforeach ()
endif ()
