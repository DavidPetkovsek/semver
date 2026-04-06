// fuzz_version.cpp — Fuzz the Version(string_view) constructor.
// Valid inputs: exercises comparison, hash, bumps, truncate, to_string.
// Invalid inputs: must throw std::invalid_argument (no UB, no crash).

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "semver/semver.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) { // NOLINT(readability-identifier-naming)
    // Cap input length to avoid spending cycles on absurdly long strings.
    if (size > 512) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

    try {
        semver::Version v(input);

        // Exercise the API surface to catch post-parse issues.
        (void)v.to_string();
        (void)v.hash();
        (void)v.major();
        (void)v.minor();
        (void)v.patch();
        (void)v.prerelease();
        (void)v.build();

        (void)v.next_major();
        (void)v.next_minor();
        (void)v.next_patch();

        (void)v.truncate("major");
        (void)v.truncate("minor");
        (void)v.truncate("patch");

        // Round-trip: to_string → parse again
        semver::Version v2(v.to_string());
        (void)(v == v2);
        (void)(v <=> v2);

    } catch (const std::invalid_argument&) {
        // Expected for malformed input.
    }

    return 0;
}