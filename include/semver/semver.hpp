// semver.hpp — C++20 Semantic Versioning Library
// A faithful translation of the python-semanticversion, with deprecated features removed.
// Copyright (c) 2025. BSD-2-Clause.

#pragma once

#include <compare>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <ostream>
#include <tuple>
#include <variant>
#include <vector>

// ---------------------------------------------------------------------------
// Export / import macros
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

// ============================================================================
// Identifier types (for SemVer precedence ordering)
// ============================================================================
struct SEMVER_API MaxIdentifier {
    auto operator<=>(const MaxIdentifier&) const = default;
};
struct SEMVER_API NumericIdentifier {
    int value;
    auto operator<=>(const NumericIdentifier&) const = default;
};
struct SEMVER_API AlphaIdentifier {
    std::string value;
    auto operator<=>(const AlphaIdentifier&) const = default;
};

using Identifier = std::variant<NumericIdentifier, AlphaIdentifier, MaxIdentifier>;

SEMVER_API extern bool operator==(const Identifier& a, const Identifier& b);
SEMVER_API extern std::strong_ordering operator<=>(const Identifier& a, const Identifier& b);

// ============================================================================
// Forward declarations
// ============================================================================
class Version;
class SimpleSpec;
class NpmSpec;

// Opaque clause pointer — concrete clause types are internal to the .cpp.
class Clause;
using ClausePtr = std::shared_ptr<Clause>;

// ============================================================================
// Version
// ============================================================================
class SEMVER_API Version {
public:
    // --- Constructors ---
    Version();
    explicit Version(std::string_view version_string);
    Version(int major,
            int minor = 0,
            int patch = 0,
            std::vector<std::string> prerelease = {},
            std::vector<std::string> build = {});

    // --- Accessors ---
    [[nodiscard]] int major() const;
    [[nodiscard]] int minor() const;
    [[nodiscard]] int patch() const;
    [[nodiscard]] const std::vector<std::string>& prerelease() const;
    [[nodiscard]] const std::vector<std::string>& build() const;

    // --- String conversion ---
    [[nodiscard]] std::string to_string() const;
    friend SEMVER_API std::ostream& operator<<(std::ostream& os, const Version& v);

    // --- Comparison ---
    [[nodiscard]] bool operator==(const Version& o) const;
    [[nodiscard]] std::weak_ordering operator<=>(const Version& o) const;

    // --- Hash ---
    [[nodiscard]] std::size_t hash() const;
    [[nodiscard]] const auto& precedence_key() const { return sort_key_; }

    // --- Bumps ---
    [[nodiscard]] Version next_major() const;
    [[nodiscard]] Version next_minor() const;
    [[nodiscard]] Version next_patch() const;

    // --- Truncate ---
    [[nodiscard]] Version truncate(std::string_view level = "patch") const;

    // --- Coerce ---
    [[nodiscard]] static Version coerce(std::string_view version_string);

    // --- Validate ---
    [[nodiscard]] static bool validate(std::string_view version_string);

private:
    int major_{};
    int minor_{};
    int patch_{};
    std::vector<std::string> prerelease_{};
    std::vector<std::string> build_{};

    std::tuple<int, int, int, std::vector<Identifier>> cmp_key_;
    std::tuple<int, int, int, std::vector<Identifier>, std::vector<Identifier>> sort_key_;

    void rebuild_keys();
    [[nodiscard]] std::vector<Identifier> build_prerelease_key() const;
    [[nodiscard]] std::vector<Identifier> build_build_key() const;
    void validate_identifiers() const;
};

SEMVER_API extern std::ostream& operator<<(std::ostream& os, const semver::Version& v);

} // namespace semver

// std::hash specialization
template<>
struct std::hash<semver::Version> {
    std::size_t operator()(const semver::Version& v) const noexcept { return v.hash(); }
};

namespace semver {

// ============================================================================
// BaseSpec / SimpleSpec / NpmSpec
// ============================================================================
class SEMVER_API BaseSpec {
public:
    [[nodiscard]] bool match(const Version& v) const;
    [[nodiscard]] std::vector<Version> filter(const std::vector<Version>& versions) const;
    [[nodiscard]] std::optional<Version> select(const std::vector<Version>& versions) const;
    [[nodiscard]] bool contains(const Version& v) const;
    bool operator==(const BaseSpec& o) const;
    [[nodiscard]] std::size_t hash() const;
    [[nodiscard]] const std::string& str() const;
    friend SEMVER_API std::ostream& operator<<(std::ostream& os, const BaseSpec& s);

protected:
    BaseSpec() = default;
    BaseSpec(std::string expr, ClausePtr c);

    std::string expression_;
    ClausePtr clause_;
};

SEMVER_API extern std::ostream& operator<<(std::ostream& os, const semver::BaseSpec& s);

class SEMVER_API SimpleSpec : public BaseSpec {
public:
    explicit SimpleSpec(std::string_view expression);
};

class SEMVER_API NpmSpec : public BaseSpec {
public:
    explicit NpmSpec(std::string_view expression);
};

// ============================================================================
// Free functions
// ============================================================================
SEMVER_API extern std::weak_ordering compare(std::string_view v1, std::string_view v2);
SEMVER_API extern bool match(std::string_view spec, std::string_view version);
SEMVER_API extern bool validate(std::string_view version_string);
SEMVER_API extern bool attempt_parse(std::string_view version_string, Version &output) noexcept;
SEMVER_API extern bool attempt_parse(std::string_view version_string, Version &output, std::string &reason) noexcept;

} // namespace semver

// std::hash specializations for specs
template<>
struct std::hash<semver::SimpleSpec> {
    std::size_t operator()(const semver::SimpleSpec& s) const noexcept { return s.hash(); }
};
template<>
struct std::hash<semver::NpmSpec> {
    std::size_t operator()(const semver::NpmSpec& s) const noexcept { return s.hash(); }
};