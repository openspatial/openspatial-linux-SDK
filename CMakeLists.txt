cmake_minimum_required(VERSION 2.8)

project(testbed)

set(CMAKE_MODULE_PATH ${testbed_SOURCE_DIR}/cmake/modules ${CMAKE_MODULE_PATH})

# Set the default install path to be './build/dist'
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) 
    set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/dist CACHE PATH "testbed install prefix" FORCE) 
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT) 

OPTION( BUILD_FRAMEWORK                "Build framework"                OFF )

SET (CMAKE_C_FLAGS                "-g -Wall -std=c99")
include(MacroOutOfSourceBuild)
include(TargetDistclean)

macro_ensure_out_of_source_build("${PROJECT_NAME} requires an out of source build.")
 
add_subdirectory(src)

install(DIRECTORY include/nodlabs DESTINATION include)

if (BUILD_FRAMEWORK)
    add_subdirectory(deps/bluez)
    add_subdirectory(framework)
endif ()

# CTest is flaky - Create a target to run our test suite directly
# add_custom_target(test COMMAND ${PROJECT_BINARY_DIR}/test/Test)
