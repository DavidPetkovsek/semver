// semver.hpp — C++20 Semantic Versioning Library
// A faithful translation of the python-semanticversion, with deprecated features removed.
// Copyright (c) 2025. BSD-2-Clause.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string_view>
#include <ostream>

#include "semver/detail.hpp"

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

    // --- Parse ---
    [[nodiscard]] static std::tuple<int, int, int,
                                    std::vector<std::string>,
                                    std::vector<std::string>>
    parse(std::string_view version_string);

    [[nodiscard]] static bool validate(std::string_view version_string);

private:
    int major_{};
    int minor_{};
    int patch_{};
    std::vector<std::string> prerelease_{};
    std::vector<std::string> build_{};

    std::tuple<int, int, int, std::vector<detail::Identifier>> cmp_key_;
    std::tuple<int, int, int, std::vector<detail::Identifier>, std::vector<detail::Identifier>> sort_key_;

    void rebuild_keys();
    [[nodiscard]] std::vector<detail::Identifier> build_prerelease_key() const;
    [[nodiscard]] std::vector<detail::Identifier> build_build_key() const;
    static void validate_identifiers(const std::vector<std::string>& ids, bool allow_leading_zeroes);
    void validate_kwargs() const;
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
    friend SEMVER_API std::ostream& operator<<(std::ostream& os, const BaseSpec& s);

protected:
    BaseSpec() = default;
    BaseSpec(std::string expr, ClausePtr c);
};

SEMVER_API extern std::ostream& operator<<(std::ostream& os, const semver::BaseSpec& s);

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
SEMVER_API extern std::weak_ordering compare(std::string_view v1, std::string_view v2);
SEMVER_API extern bool match(std::string_view spec, std::string_view version);
SEMVER_API extern bool validate(std::string_view version_string);
SEMVER_API extern bool attempt_parse(std::string_view version_string, Version &output) noexcept;

} // namespace semver