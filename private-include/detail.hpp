// detail.hpp — Internal helpers for semver
// This header is NOT installed. It is a private implementation detail.
// Copyright (c) 2025. BSD-2-Clause.

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <semver/semver.hpp>

namespace semver {
namespace detail {

// ---------------------------------------------------------------------------
// Character / string utilities
// ---------------------------------------------------------------------------
extern bool is_digit(char c);
extern bool is_alnum_or_hyphen(char c);
extern bool is_digit_string(std::string_view s);
extern bool has_leading_zero(std::string_view v);
extern std::vector<std::string> split(std::string_view s, char delim);
extern std::string join(const std::vector<std::string>& v, std::string_view sep);
extern int parse_int(std::string_view s);
extern std::string lstrip_zeros(std::string_view s);
extern std::string_view trim(std::string_view s);
extern std::string_view consume_digits(std::string_view s, std::size_t& pos);
extern std::string_view consume_identifiers(std::string_view s, std::size_t& pos);

// ---------------------------------------------------------------------------
// Version-string parser parts (hand-written, no regex)
// ---------------------------------------------------------------------------
struct VersionParts {
    std::string_view major_s, minor_s, patch_s;
    bool has_minor = false, has_patch = false;
    bool has_prerelease = false, has_build = false;
    std::string_view prerelease_s, build_s;
};

extern VersionParts parse_version_parts(std::string_view s);

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

extern std::string_view consume_prefix(std::string_view s, std::size_t& pos);
extern bool is_wildcard(std::string_view s);
extern std::string_view consume_number_or_wildcard(std::string_view s, std::size_t& pos);
extern SpecParts parse_spec_block(std::string_view s, bool allow_v_prefix = false);

// ---------------------------------------------------------------------------
// Identifier factory
// ---------------------------------------------------------------------------
extern Identifier make_identifier(std::string_view part);

} // namespace detail
} // namespace semver