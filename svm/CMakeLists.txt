add_library(odr_svm STATIC
        src/Svm2Svg.cpp
        )
target_include_directories(odr_svm
        PUBLIC
        include
        PRIVATE
        src
        )
target_link_libraries(odr_svm PRIVATE glog)
set_property(TARGET odr_svm PROPERTY POSITION_INDEPENDENT_CODE ON)
