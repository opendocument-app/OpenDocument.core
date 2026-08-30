# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/utf8cpp-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${utfcpp_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${utf8cpp_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET utf8cpp::utf8cpp)
    add_library(utf8cpp::utf8cpp INTERFACE IMPORTED)
    message(${utf8cpp_MESSAGE_MODE} "Conan: Target declared 'utf8cpp::utf8cpp'")
endif()
if(NOT TARGET utf8::cpp)
    add_library(utf8::cpp INTERFACE IMPORTED)
    set_property(TARGET utf8::cpp PROPERTY INTERFACE_LINK_LIBRARIES utf8cpp::utf8cpp)
endif()
if(NOT TARGET utf8cpp)
    add_library(utf8cpp INTERFACE IMPORTED)
    set_property(TARGET utf8cpp PROPERTY INTERFACE_LINK_LIBRARIES utf8cpp::utf8cpp)
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/utf8cpp-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()