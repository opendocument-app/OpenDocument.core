include(FetchContent)

# cryptopp: fixes "phony targets" duplicate error during Android build
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
# miniz
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
# uchardet
set(BUILD_BINARY OFF CACHE BOOL "" FORCE)

include(pugixml.cmake)
include(miniz.cmake)
include(cryptopp.cmake)
include(json.cmake)
include(uchardet.cmake)
include(csv.cmake)
include(googletest.cmake)
