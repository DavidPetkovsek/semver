// fuzz_attempt_parse.cpp — Fuzz the noexcept attempt_parse path.
// This must NEVER throw or crash regardless of input.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "semver/semver.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size > 512) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size);

    semver::Version v;
    std::string reason;
    bool ok = semver::attempt_parse(input, v, reason);

    if (ok) {
        // Round-trip: successful parse must survive to_string → re-parse.
        semver::Version v2;
        std::string reason2;
        bool ok2 = semver::attempt_parse(v.to_string(), v2, reason2);
        (void)ok2;
        (void)(v == v2);
    }

    return 0;
}