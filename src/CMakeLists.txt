# Include the directory itself as a path to include directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(testbed_SOURCES
    eir.c eir.h
    anki_util.c
    advertisement.c advertisement.h
    uuid.c uuid.h
    protocol.c protocol.h
)


# For a large number of source files you can create it in a simpler way
# using file() function:
# file(GLOB drivekit_SOURCES *.cpp)

add_library(nodlabs ${testbed_SOURCES})

install(TARGETS nodlabs
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
