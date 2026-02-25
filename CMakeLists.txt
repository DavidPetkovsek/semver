cmake_minimum_required(VERSION 3.21)

# --------------------------------------------------------------------------
# vcpkg toolchain — auto-detected if VCPKG_ROOT is set, or pass
# -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
# --------------------------------------------------------------------------

project(semver
    VERSION   0.2.1
    LANGUAGES CXX
    DESCRIPTION "C++20 Semantic Versioning Library — a faithful translation of python-semanticversion, with deprecated features removed."
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
option(SEMVER_BUILD_SHARED   "Build semver as a shared library (OFF = static)" ON)
option(SEMVER_BUILD_TESTS    "Build the Catch2 test suite" OFF)

# --------------------------------------------------------------------------
# Library target
# --------------------------------------------------------------------------
# Compiled (STATIC or SHARED) — a thin .cpp that includes the header
# so the regex statics / any future .cpp TUs are compiled once.
if(SEMVER_BUILD_SHARED)
    add_library(semver SHARED src/semver.cpp)
    target_compile_definitions(semver
        PUBLIC  SEMVER_SHARED
        PRIVATE SEMVER_EXPORTS
    )
else()
    add_library(semver STATIC src/semver.cpp)
endif()
add_library(semver::semver ALIAS semver)

target_include_directories(semver
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(semver PUBLIC cxx_std_20)

set_target_properties(semver PROPERTIES
    VERSION                    ${PROJECT_VERSION}
    SOVERSION                  ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE  ON
    CXX_VISIBILITY_PRESET      hidden
    VISIBILITY_INLINES_HIDDEN  ON
)

# On Windows, export all symbols for shared builds.
if(SEMVER_BUILD_SHARED AND WIN32)
    set_target_properties(semver PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# --------------------------------------------------------------------------
# Tests
# --------------------------------------------------------------------------
if(SEMVER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --------------------------------------------------------------------------
# Install / Export
# --------------------------------------------------------------------------
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Install the header.
install(
    FILES
        include/semver/semver.hpp
        include/semver/detail.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/semver
)

# Install the target.
install(
    TARGETS semver
    EXPORT  semverTargets
    LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export target file.
install(
    EXPORT      semverTargets
    FILE        semverTargets.cmake
    NAMESPACE   semver::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/semver
)

# Package config.
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/semverConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/semverConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/semver
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/semverConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/semverConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/semverConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/semver
)
