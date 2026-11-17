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
    test_bounded_queue.cpp
    test_concurrency.cpp
    test_config.cpp
    test_device_controller.cpp
    test_device_state.cpp
    test_logger.cpp
    test_positioning.cpp
    test_sample_handler.cpp
    test_scanning.cpp
)
target_link_libraries(medsync_tests PRIVATE medsync_core GTest::gtest_main medsync_warnings)

include(GoogleTest)
gtest_discover_tests(medsync_tests PROPERTIES TIMEOUT 30)
