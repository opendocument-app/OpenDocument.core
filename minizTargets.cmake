# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/miniz-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${miniz_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${miniz_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET miniz::miniz)
    add_library(miniz::miniz INTERFACE IMPORTED)
    message(${miniz_MESSAGE_MODE} "Conan: Target declared 'miniz::miniz'")
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/miniz-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()