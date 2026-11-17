include(FetchContent)

# Pin GoogleTest to a release so that builds are reproducible.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.18.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(medsync_tests
    test_device_state.cpp
)
target_link_libraries(medsync_tests PRIVATE medsync_core GTest::gtest_main medsync_warnings)

include(GoogleTest)
gtest_discover_tests(medsync_tests)
