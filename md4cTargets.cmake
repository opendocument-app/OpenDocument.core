# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/md4c-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${md4c_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${md4c_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET md4c::md4c-html)
    add_library(md4c::md4c-html INTERFACE IMPORTED)
    message(${md4c_MESSAGE_MODE} "Conan: Target declared 'md4c::md4c-html'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/md4c-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()