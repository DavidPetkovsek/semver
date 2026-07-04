// fuzz_coerce.cpp — Fuzz Version::coerce, which accepts looser input formats.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include "dpetkov-semver/semver.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 512) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size);

    try {
        semver::Version v = semver::Version::coerce(input);

        (void)v.to_string();
        (void)v.hash();

        // Coerced result must be a valid version.
        semver::Version v2(v.to_string());
        (void)(v == v2);

    } catch (const std::invalid_argument&) {
        // Expected for truly uncoercible input.
    }

    return 0;
}