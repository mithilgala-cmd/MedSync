cmake_minimum_required(VERSION 3.20)

project(MedSync VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared warning settings. Kept as an interface target so that third-party
# code (GoogleTest) is not compiled with our stricter flags.
add_library(medsync_warnings INTERFACE)
if(MSVC)
    target_compile_options(medsync_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(medsync_warnings INTERFACE -Wall -Wextra -Wpedantic)
endif()

add_library(medsync_core
    src/Config.cpp
    src/DeviceState.cpp
    src/Logger.cpp
    src/PositioningController.cpp
    src/SampleHandler.cpp
    src/ScanningModule.cpp
    src/Types.cpp
)
target_include_directories(medsync_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(medsync_core PUBLIC cxx_std_17)
target_link_libraries(medsync_core PRIVATE medsync_warnings)

add_executable(medsync src/main.cpp)
target_link_libraries(medsync PRIVATE medsync_core medsync_warnings)

option(MEDSYNC_BUILD_TESTS "Build MedSync unit tests" ON)
if(MEDSYNC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
