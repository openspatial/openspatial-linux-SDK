# Include the directory itself as a path to include directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)

include(FindGLIB2)

include_directories(
    ${bluez_SOURCE_DIR}/include    
    ${GLIB2_INCLUDE_DIRS}
)

set(bluez_SOURCES
att.c bluetooth.c btio.c gatt.c gattrib.c hci.c log.h log.c sdp.c utils.c uuid.c
)

add_library(bluez ${bluez_SOURCES})
target_link_libraries(bluez
                    ${GLIB2_LIBRARIES}
                    )

install(TARGETS bluez
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
