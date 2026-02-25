vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO DavidPetkovsek/semver
    REF "v${VERSION}"
    SHA512 0
    HEAD_REF main
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" SEMVER_SHARED)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSEMVER_BUILD_SHARED=${SEMVER_SHARED}
        -DSEMVER_BUILD_TESTS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(
    PACKAGE_NAME semver
    CONFIG_PATH lib/cmake/semver
)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")