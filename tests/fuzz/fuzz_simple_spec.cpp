// fuzz_simple_spec.cpp — Fuzz SimpleSpec parsing and matching.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "semver/semver.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 512) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size);

    try {
        semver::SimpleSpec spec(input);

        (void)spec.str();
        (void)spec.hash();
        (void)spec.min_version();

        // Test against a few known versions.
        (void)spec.match(semver::Version("0.0.0"));
        (void)spec.match(semver::Version("1.0.0"));
        (void)spec.match(semver::Version("1.0.0-alpha"));
        (void)spec.match(semver::Version("99.99.99"));

    } catch (const std::invalid_argument&) {
        // Expected for malformed specs.
    }

    return 0;
}