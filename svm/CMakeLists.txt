add_library(odr_svm OBJECT
        src/Svm2Svg.cpp
)
target_include_directories(odr_svm PUBLIC include)
target_link_libraries(odr_svm PRIVATE glog)