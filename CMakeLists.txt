cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CMAKE_OSX_ARCHITECTURES arm64)
endif()

project(InputBuffer VERSION 0.1.0)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Geode's iOS loader looks for the binary using the mod ID, not the CMake
# project name. Ensure the packaged dylib is named accordingly.
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "ultimatemjl.input-buffer"
)

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK. Define GEODE_SDK.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)
setup_geode_mod(${PROJECT_NAME})

# The iOS-generated bindings refer to FMOD types without including their declarations.
# Force-include the SDK's bundled FMOD header for the bindings target only.
if (CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if (TARGET GeodeBindings)
        target_compile_options(GeodeBindings PRIVATE
            "SHELL:-include $ENV{GEODE_SDK}/loader/include/Geode/fmod/fmod.hpp"
        )
    endif()
endif()
