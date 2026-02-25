// semver.hpp — C++20 Semantic Versioning Library
// A faithful translation of the python-semanticversion library.
// Copyright (c) 2025. BSD-2-Clause.

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
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
// Forward declarations
// ============================================================================
class Version;
class Clause;
class SimpleSpec;
class NpmSpec;

using ClausePtr = std::shared_ptr<Clause>;

// ============================================================================
// Utility helpers (detail)
// ============================================================================
namespace detail {

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
SEMVER_API bool operator<(const Identifier& a, const Identifier& b);
SEMVER_API bool operator<=(const Identifier& a, const Identifier& b);
SEMVER_API bool operator>(const Identifier& a, const Identifier& b);
SEMVER_API bool operator>=(const Identifier& a, const Identifier& b);
SEMVER_API Identifier make_identifier(std::string_view part);

} // namespace detail

// ============================================================================
// Version
// ============================================================================
class SEMVER_API Version {
public:
    int major{};
    std::optional<int> minor{};
    std::optional<int> patch{};
    std::optional<std::vector<std::string>> prerelease{};
    std::optional<std::vector<std::string>> build{};
    bool partial{false};

    // --- Constructors ---
    Version();
    explicit Version(std::string_view version_string, bool partial = false);
    Version(int major,
            std::optional<int> minor,
            std::optional<int> patch,
            std::optional<std::vector<std::string>> prerelease = std::vector<std::string>{},
            std::optional<std::vector<std::string>> build = std::nullopt,
            bool partial = false);

    // --- String conversion ---
    [[nodiscard]] std::string to_string() const;
    friend std::ostream& operator<<(std::ostream& os, const Version& v);

    // --- Comparison ---
    bool operator==(const Version& o) const;
    bool operator!=(const Version& o) const;
    bool operator<(const Version& o) const;
    bool operator<=(const Version& o) const;
    bool operator>(const Version& o) const;
    bool operator>=(const Version& o) const;
    [[nodiscard]] int cmp(const Version& o) const;

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
    [[nodiscard]] static Version coerce(std::string_view version_string, bool partial = false);

    // --- Parse ---
    [[nodiscard]] static std::tuple<int,
                                    std::optional<int>,
                                    std::optional<int>,
                                    std::optional<std::vector<std::string>>,
                                    std::optional<std::vector<std::string>>>
    parse(std::string_view version_string, bool partial = false);

    [[nodiscard]] static bool validate(std::string_view version_string);

private:
    std::tuple<int, int, int, std::vector<detail::Identifier>> cmp_key_;
    std::tuple<int, int, int, std::vector<detail::Identifier>, std::vector<detail::Identifier>> sort_key_;

    void rebuild_keys();
    [[nodiscard]] std::vector<detail::Identifier> build_prerelease_key() const;
    [[nodiscard]] std::vector<detail::Identifier> build_build_key() const;
    static void validate_identifiers(const std::vector<std::string>& ids, bool allow_leading_zeroes);
    void validate_kwargs() const;
};

} // namespace semver

// std::hash specialisation
template<>
struct std::hash<semver::Version> {
    std::size_t operator()(const semver::Version& v) const noexcept { return v.hash(); }
};

namespace semver {

// ============================================================================
// Clause hierarchy
// ============================================================================
class SEMVER_API Clause : public std::enable_shared_from_this<Clause> {
public:
    virtual ~Clause() = default;
    [[nodiscard]] virtual bool match(const Version& version) const = 0;
    [[nodiscard]] virtual bool operator==(const Clause& o) const = 0;
    [[nodiscard]] virtual std::size_t hash_value() const = 0;
    [[nodiscard]] ClausePtr and_with(ClausePtr other) const;
    [[nodiscard]] ClausePtr or_with(ClausePtr other) const;
};

class SEMVER_API Never : public Clause {
public:
    [[nodiscard]] bool match(const Version&) const override;
    [[nodiscard]] bool operator==(const Clause& o) const override;
    [[nodiscard]] std::size_t hash_value() const override;
};

class SEMVER_API Always : public Clause {
public:
    [[nodiscard]] bool match(const Version&) const override;
    [[nodiscard]] bool operator==(const Clause& o) const override;
    [[nodiscard]] std::size_t hash_value() const override;
};

class SEMVER_API Range : public Clause {
public:
    enum class Op { EQ, GT, GTE, LT, LTE, NEQ };
    enum class PrePolicy { ALWAYS, NATURAL, SAME_PATCH };
    enum class BuildPolicy { IMPLICIT, STRICT };

    Op op;
    Version target;
    PrePolicy pre_policy;
    BuildPolicy build_policy;

    Range(Op op, Version target, PrePolicy pp = PrePolicy::NATURAL, BuildPolicy bp = BuildPolicy::IMPLICIT);

    [[nodiscard]] bool match(const Version& version) const override;
    [[nodiscard]] bool operator==(const Clause& o) const override;
    [[nodiscard]] std::size_t hash_value() const override;
};

class SEMVER_API AllOf : public Clause {
public:
    std::vector<ClausePtr> clauses;
    explicit AllOf(std::vector<ClausePtr> c);
    AllOf(std::initializer_list<ClausePtr> c);

    [[nodiscard]] bool match(const Version& version) const override;
    [[nodiscard]] bool operator==(const Clause& o) const override;
    [[nodiscard]] std::size_t hash_value() const override;
};

class SEMVER_API AnyOf : public Clause {
public:
    std::vector<ClausePtr> clauses;
    explicit AnyOf(std::vector<ClausePtr> c);
    AnyOf(std::initializer_list<ClausePtr> c);

    [[nodiscard]] bool match(const Version& version) const override;
    [[nodiscard]] bool operator==(const Clause& o) const override;
    [[nodiscard]] std::size_t hash_value() const override;
};

// --- Clause factory helpers ---
SEMVER_API ClausePtr make_never();
SEMVER_API ClausePtr make_always();
SEMVER_API ClausePtr make_range(Range::Op op, Version target,
                                Range::PrePolicy pp = Range::PrePolicy::NATURAL,
                                Range::BuildPolicy bp = Range::BuildPolicy::IMPLICIT);

// ============================================================================
// BaseSpec / SimpleSpec / NpmSpec
// ============================================================================
class SEMVER_API BaseSpec {
public:
    std::string expression;
    ClausePtr clause;

    [[nodiscard]] bool match(const Version& v) const;
    [[nodiscard]] std::vector<Version> filter(const std::vector<Version>& versions) const;
    [[nodiscard]] std::optional<Version> select(const std::vector<Version>& versions) const;
    [[nodiscard]] bool contains(const Version& v) const;
    bool operator==(const BaseSpec& o) const;
    [[nodiscard]] std::size_t hash() const;
    [[nodiscard]] const std::string& str() const;
    friend std::ostream& operator<<(std::ostream& os, const BaseSpec& s);

protected:
    BaseSpec() = default;
    BaseSpec(std::string expr, ClausePtr c);
};

class SEMVER_API SimpleSpec : public BaseSpec {
public:
    explicit SimpleSpec(std::string_view expression);

private:
    [[nodiscard]] static ClausePtr parse_expression(std::string_view expression);
    static ClausePtr parse_block(std::string_view expr);
};

class SEMVER_API NpmSpec : public BaseSpec {
public:
    explicit NpmSpec(std::string_view expression);

private:
    static ClausePtr npm_range(Range::Op op, Version target);
    static ClausePtr parse_expression(std::string_view expression);
    static std::vector<ClausePtr> parse_simple(std::string_view simple);
    static std::vector<std::string_view> split_joiner(std::string_view s);
};

// ============================================================================
// Free functions
// ============================================================================
SEMVER_API int compare(std::string_view v1, std::string_view v2);
SEMVER_API bool match(std::string_view spec, std::string_view version);
SEMVER_API bool validate(std::string_view version_string);

} // namespace semver
