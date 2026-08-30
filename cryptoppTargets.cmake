# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/cryptopp-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${cryptopp_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${cryptopp_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET cryptopp::cryptopp)
    add_library(cryptopp::cryptopp INTERFACE IMPORTED)
    message(${cryptopp_MESSAGE_MODE} "Conan: Target declared 'cryptopp::cryptopp'")
endif()
if(NOT TARGET cryptopp-static)
    add_library(cryptopp-static INTERFACE IMPORTED)
    set_property(TARGET cryptopp-static PROPERTY INTERFACE_LINK_LIBRARIES cryptopp::cryptopp)
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/cryptopp-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()