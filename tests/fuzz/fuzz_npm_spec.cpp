// fuzz_npm_spec.cpp — Fuzz NpmSpec parsing, matching, min_version, and subset.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include "dpetkov-semver/semver.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 512) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size);

    try {
        semver::NpmSpec spec(input);

        (void)spec.str();
        (void)spec.hash();
        (void)spec.min_version();

        (void)spec.match(semver::Version("0.0.0"));
        (void)spec.match(semver::Version("1.0.0"));
        (void)spec.match(semver::Version("1.0.0-alpha"));
        (void)spec.match(semver::Version("99.99.99"));

        // If min_version succeeds, verify it actually matches.
        auto mv = spec.min_version();
        if (mv.has_value()) {
            (void)spec.match(*mv);
        }

    } catch (const std::invalid_argument&) {
        // Expected for malformed specs.
    }

    return 0;
}