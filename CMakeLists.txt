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

add_executable(medsync src/main.cpp)
target_link_libraries(medsync PRIVATE medsync_warnings)
