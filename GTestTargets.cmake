# Load the debug and release variables
file(GLOB DATA_FILES "${CMAKE_CURRENT_LIST_DIR}/GTest-*-data.cmake")

foreach(f ${DATA_FILES})
    include(${f})
endforeach()

# Create the targets for all the components
foreach(_COMPONENT ${gtest_COMPONENT_NAMES} )
    if(NOT TARGET ${_COMPONENT})
        add_library(${_COMPONENT} INTERFACE IMPORTED)
        message(${GTest_MESSAGE_MODE} "Conan: Component target declared '${_COMPONENT}'")
    endif()
endforeach()

if(NOT TARGET gtest::gtest)
    add_library(gtest::gtest INTERFACE IMPORTED)
    message(${GTest_MESSAGE_MODE} "Conan: Target declared 'gtest::gtest'")
endif()
if(NOT TARGET GTest::GTest)
    add_library(GTest::GTest INTERFACE IMPORTED)
    set_property(TARGET GTest::GTest PROPERTY INTERFACE_LINK_LIBRARIES GTest::gtest)
endif()
if(NOT TARGET GTest::Main)
    add_library(GTest::Main INTERFACE IMPORTED)
    set_property(TARGET GTest::Main PROPERTY INTERFACE_LINK_LIBRARIES GTest::gtest_main)
endif()
# Load the debug and release library finders
file(GLOB CONFIG_FILES "${CMAKE_CURRENT_LIST_DIR}/GTest-Target-*.cmake")

foreach(f ${CONFIG_FILES})
    include(${f})
endforeach()