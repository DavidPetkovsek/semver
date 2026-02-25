// detail.hpp — Internal helpers for semver.hpp
// Not intended for direct use by consumers.
// Copyright (c) 2025. BSD-2-Clause.

#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Export / import macros (must match semver.hpp)
// ---------------------------------------------------------------------------
#if defined(SEMVER_SHARED)
#  if defined(_MSC_VER)
#    if defined(SEMVER_EXPORTS)
#      define SEMVER_API __declspec(dllexport)
#    else
#      define SEMVER_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define SEMVER_API __attribute__((visibility("default")))
#  else
#    define SEMVER_API
#  endif
#else
#  define SEMVER_API
#endif

namespace semver {
namespace detail {

// ---------------------------------------------------------------------------
// Character / string utilities
// ---------------------------------------------------------------------------
SEMVER_API bool is_digit(char c);
SEMVER_API bool is_alnum_or_hyphen(char c);
SEMVER_API bool is_digit_string(std::string_view s);
SEMVER_API bool has_leading_zero(std::string_view v);
SEMVER_API std::vector<std::string> split(std::string_view s, char delim);
SEMVER_API std::string join(const std::vector<std::string>& v, std::string_view sep);
SEMVER_API int parse_int(std::string_view s);
SEMVER_API std::string lstrip_zeros(std::string_view s);
SEMVER_API std::string_view trim(std::string_view s);
SEMVER_API std::string_view consume_digits(std::string_view s, std::size_t& pos);
SEMVER_API std::string_view consume_identifiers(std::string_view s, std::size_t& pos);

// ---------------------------------------------------------------------------
// Version-string parser parts (hand-written, no regex)
// ---------------------------------------------------------------------------
struct VersionParts {
    std::string_view major_s, minor_s, patch_s;
    bool has_minor = false, has_patch = false;
    bool has_prerelease = false, has_build = false;
    std::string_view prerelease_s, build_s;
};

SEMVER_API VersionParts parse_version_parts(std::string_view s, bool partial = false);

// ---------------------------------------------------------------------------
// Spec-expression parser parts
// ---------------------------------------------------------------------------
struct SpecParts {
    std::string_view prefix;
    std::string_view major_s, minor_s, patch_s;
    bool has_minor = false, has_patch = false;
    bool has_prerelease = false, has_build = false;
    std::string_view prerelease_s, build_s;
};

SEMVER_API std::string_view consume_prefix(std::string_view s, std::size_t& pos);
SEMVER_API bool is_wildcard(std::string_view s);
SEMVER_API std::string_view consume_number_or_wildcard(std::string_view s, std::size_t& pos);
SEMVER_API SpecParts parse_spec_block(std::string_view s, bool allow_v_prefix = false);

// ---------------------------------------------------------------------------
// Prerelease / Build Identifier types (for SemVer precedence ordering)
// ---------------------------------------------------------------------------
struct MaxIdentifier {
    auto operator<=>(const MaxIdentifier&) const = default;
};
struct NumericIdentifier {
    int value;
    auto operator<=>(const NumericIdentifier&) const = default;
};
struct AlphaIdentifier {
    std::string value;
    auto operator<=>(const AlphaIdentifier&) const = default;
};

using Identifier = std::variant<NumericIdentifier, AlphaIdentifier, MaxIdentifier>;

SEMVER_API bool operator==(const Identifier& a, const Identifier& b);
SEMVER_API std::strong_ordering operator<=>(const Identifier& a, const Identifier& b);
SEMVER_API Identifier make_identifier(std::string_view part);

} // namespace detail
} // namespace semver