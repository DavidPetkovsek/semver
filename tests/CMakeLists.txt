include(FetchContent)

# Try vcpkg / system-installed Catch2 first; fall back to FetchContent.
find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
    message(STATUS "Catch2 not found via find_package — fetching with FetchContent")
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.7.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    # Catch2::Catch (the CMake helper module) lives inside the fetched source tree.
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

add_executable(semver_tests
    test_semver.cpp
)

target_link_libraries(semver_tests
    PRIVATE
        semver::semver
        Catch2::Catch2WithMain
)

# Auto-discover Catch2 TEST_CASEs and register them with CTest.
include(Catch)
catch_discover_tests(semver_tests)