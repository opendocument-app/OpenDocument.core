# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/uchardet-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${uchardet_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${uchardet_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET uchardet::uchardet)
    add_library(uchardet::uchardet INTERFACE IMPORTED)
    message(${uchardet_MESSAGE_MODE} "Conan: Target declared 'uchardet::uchardet'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/uchardet-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()