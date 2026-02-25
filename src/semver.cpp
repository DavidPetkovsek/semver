// semver.cpp — Implementation of semver.hpp
// Copyright (c) 2025. BSD-2-Clause.

#include "semver/semver.hpp"

#include <algorithm>
#include <charconv>
#include <compare>
#include <stdexcept>
#include <tuple>
#include <string>

namespace semver {

// ============================================================================
// detail helpers
// ============================================================================
namespace detail {

/*extern*/ bool is_digit(char c) { return c >= '0' && c <= '9'; }

/*extern*/ bool is_alnum_or_hyphen(char c) {
    return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-';
}

/*extern*/ bool is_digit_string(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), is_digit);
}

/*extern*/ bool has_leading_zero(std::string_view v) {
    return v.size() > 1 && v[0] == '0' && is_digit_string(v);
}

/*extern*/ std::vector<std::string> split(std::string_view s, char delim) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            parts.emplace_back(std::move(current));
            current.clear();
        } else {
            current += c;
        }
    }
    parts.emplace_back(std::move(current));
    return parts;
}

/*extern*/ std::string join(const std::vector<std::string>& v, std::string_view sep) {
    if (v.empty()) return {};
    std::string r = v[0];
    for (std::size_t i = 1; i < v.size(); ++i) {
        r += sep;
        r += v[i];
    }
    return r;
}

/*extern*/ int parse_int(std::string_view s) {
    int val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        throw std::invalid_argument("Not a valid integer: " + std::string(s));
    return val;
}

/*extern*/ std::string lstrip_zeros(std::string_view s) {
    if (s.empty()) return "0";
    auto pos = s.find_first_not_of('0');
    if (pos == std::string_view::npos) return "0";
    return std::string(s.substr(pos));
}

/*extern*/ std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

/*extern*/ std::string_view consume_digits(std::string_view s, std::size_t& pos) {
    auto start = pos;
    while (pos < s.size() && is_digit(s[pos])) ++pos;
    return s.substr(start, pos - start);
}

/*extern*/ std::string_view consume_identifiers(std::string_view s, std::size_t& pos) {
    auto start = pos;
    while (pos < s.size() && (is_alnum_or_hyphen(s[pos]) || s[pos] == '.')) ++pos;
    return s.substr(start, pos - start);
}

// ---------------------------------------------------------------------------
// Version-string parser
// ---------------------------------------------------------------------------
/*extern*/ VersionParts parse_version_parts(std::string_view s) {
    VersionParts p;
    std::size_t pos = 0;

    // Major
    p.major_s = consume_digits(s, pos);
    if (p.major_s.empty())
        throw std::invalid_argument("Invalid version string: " + std::string(s));

    if (has_leading_zero(p.major_s))
        throw std::invalid_argument("Invalid leading zero in major: " + std::string(s));

    // Minor (required)
    if (pos >= s.size() || s[pos] != '.')
        throw std::invalid_argument("Invalid version string (missing minor): " + std::string(s));
        ++pos;
        p.minor_s = consume_digits(s, pos);
    if (p.minor_s.empty())
        throw std::invalid_argument("Invalid version string (missing minor): " + std::string(s));
    p.has_minor = true;
    if (has_leading_zero(p.minor_s))
            throw std::invalid_argument("Invalid leading zero in minor: " + std::string(s));

    // Patch (required)
    if (pos >= s.size() || s[pos] != '.')
        throw std::invalid_argument("Invalid version string (missing patch): " + std::string(s));
            ++pos;
            p.patch_s = consume_digits(s, pos);
    if (p.patch_s.empty())
        throw std::invalid_argument("Invalid version string (missing patch): " + std::string(s));
    p.has_patch = true;
    if (has_leading_zero(p.patch_s))
                throw std::invalid_argument("Invalid leading zero in patch: " + std::string(s));

    // Prerelease
    if (pos < s.size() && s[pos] == '-') {
        ++pos;
        p.has_prerelease = true;
        p.prerelease_s = consume_identifiers(s, pos);
    }

    // Build
    if (pos < s.size() && s[pos] == '+') {
        ++pos;
        p.has_build = true;
        p.build_s = consume_identifiers(s, pos);
    }

    if (pos != s.size())
        throw std::invalid_argument("Invalid version string: " + std::string(s));

    return p;
}

// ---------------------------------------------------------------------------
// Spec-expression parser
// ---------------------------------------------------------------------------
/*extern*/ std::string_view consume_prefix(std::string_view s, std::size_t& pos) {
    if (pos >= s.size()) return {};
    if (pos + 1 < s.size()) {
        auto two = s.substr(pos, 2);
        if (two == "<=" || two == ">=" || two == "==" || two == "!=" || two == "~=") {
            pos += 2;
            return two;
        }
    }
    char c = s[pos];
    if (c == '<' || c == '>' || c == '=' || c == '~' || c == '^') {
        pos += 1;
        return s.substr(pos - 1, 1);
    }
    return {};
}

/*extern*/ bool is_wildcard(std::string_view s) {
    return s == "*" || s == "x" || s == "X";
}

/*extern*/ std::string_view consume_number_or_wildcard(std::string_view s, std::size_t& pos) {
    if (pos >= s.size()) return {};
    if (s[pos] == '*' || s[pos] == 'x' || s[pos] == 'X') {
        ++pos;
        return s.substr(pos - 1, 1);
    }
    return consume_digits(s, pos);
}

/*extern*/ SpecParts parse_spec_block(std::string_view s, bool allow_v_prefix) {
    SpecParts p;
    std::size_t pos = 0;

    if (allow_v_prefix && pos < s.size() && s[pos] == 'v') ++pos;

    p.prefix = consume_prefix(s, pos);

    p.major_s = consume_number_or_wildcard(s, pos);
    if (p.major_s.empty())
        throw std::invalid_argument("Invalid spec block: " + std::string(s));

    if (pos < s.size() && s[pos] == '.') {
        ++pos;
        p.minor_s = consume_number_or_wildcard(s, pos);
        p.has_minor = !p.minor_s.empty();
        if (p.has_minor && pos < s.size() && s[pos] == '.') {
            ++pos;
            p.patch_s = consume_number_or_wildcard(s, pos);
            p.has_patch = !p.patch_s.empty();
        }
    }

    if (pos < s.size() && s[pos] == '-') {
        ++pos;
        p.has_prerelease = true;
        p.prerelease_s = consume_identifiers(s, pos);
    }

    if (pos < s.size() && s[pos] == '+') {
        ++pos;
        p.has_build = true;
        p.build_s = consume_identifiers(s, pos);
    }

    if (pos != s.size())
        throw std::invalid_argument("Invalid spec block: " + std::string(s));

    return p;
}

// ---------------------------------------------------------------------------
// Identifier comparison
// ---------------------------------------------------------------------------
/*extern*/ bool operator==(const Identifier& a, const Identifier& b) {
    return a.index() == b.index() && std::visit([&](const auto& va) -> bool {
        using T = std::decay_t<decltype(va)>;
        return va == std::get<T>(b);
    }, a);
}

/*extern*/ std::strong_ordering operator<=>(const Identifier& a, const Identifier& b) {
    if (a.index() != b.index()) return a.index() <=> b.index();
    return std::visit([&](const auto& va) -> std::strong_ordering {
        using T = std::decay_t<decltype(va)>;
        return va <=> std::get<T>(b);
    }, a);
}

/*extern*/ Identifier make_identifier(std::string_view part) {
    if (is_digit_string(part))
        return NumericIdentifier{parse_int(part)};
    return AlphaIdentifier{std::string(part)};
}

} // namespace detail

// ============================================================================
// Version implementation
// ============================================================================

Version::Version() = default;

Version::Version(std::string_view version_string) {
    auto [mj, mn, pa, pr, bd] = parse(version_string);
    major_ = mj;
    minor_ = mn;
    patch_ = pa;
    prerelease_ = std::move(pr);
    build_ = std::move(bd);
    rebuild_keys();
}

Version::Version(int major, int minor, int patch,
                 std::vector<std::string> prerelease,
                 std::vector<std::string> build)
    : major_(major), minor_(minor), patch_(patch),
      prerelease_(std::move(prerelease)), build_(std::move(build))
{
    validate_kwargs();
    rebuild_keys();
}

int Version::major() const { return major_; }
int Version::minor() const { return minor_; }
int Version::patch() const { return patch_; }
const std::vector<std::string>& Version::prerelease() const { return prerelease_; }
const std::vector<std::string>& Version::build() const { return build_; }

std::string Version::to_string() const {
    std::string v = std::to_string(major_) + "." + std::to_string(minor_) + "." + std::to_string(patch_);

    if (!prerelease_.empty())
        v += "-" + detail::join(prerelease_, ".");
    if (!build_.empty())
        v += "+" + detail::join(build_, ".");
    return v;
}

/*extern*/ std::ostream& operator<<(std::ostream& os, const Version& v) {
    return os << v.to_string();
}

bool Version::operator==(const Version& o) const {
    return major_ == o.major_
        && minor_ == o.minor_
        && patch_ == o.patch_
        && prerelease_ == o.prerelease_
        && build_ == o.build_;
}

std::weak_ordering Version::operator<=>(const Version& o) const {
    auto ordering = cmp_key_ <=> o.cmp_key_;
    if (ordering != 0) return ordering;
    // Versions equal by precedence but differing in build are unordered
    if (*this == o) return std::weak_ordering::equivalent;
    return std::weak_ordering::equivalent; // std::partial_ordering::unordered;
}

std::size_t Version::hash() const {
    std::size_t h = std::hash<int>{}(major_);
    auto combine = [&](std::size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
    combine(std::hash<int>{}(minor_));
    combine(std::hash<int>{}(patch_));
    for (auto& s : prerelease_) combine(std::hash<std::string>{}(s));
    for (auto& s : build_) combine(std::hash<std::string>{}(s));
    return h;
}

Version Version::next_major() const {
    if (!prerelease_.empty() && minor_ == 0 && patch_ == 0)
        return Version(major_, 0, 0);
    return Version(major_ + 1, 0, 0);
}

Version Version::next_minor() const {
    if (!prerelease_.empty() && patch_ == 0)
        return Version(major_, minor_, 0);
    return Version(major_, minor_ + 1, 0);
}

Version Version::next_patch() const {
    if (!prerelease_.empty())
        return Version(major_, minor_, patch_);
    return Version(major_, minor_, patch_ + 1);
}

Version Version::truncate(std::string_view level) const {
    if (level == "build")
        return Version(major_, minor_, patch_, prerelease_, build_);
    if (level == "prerelease")
        return Version(major_, minor_, patch_, prerelease_);
    if (level == "patch")
        return Version(major_, minor_, patch_);
    if (level == "minor")
        return Version(major_, minor_, 0);
    if (level == "major")
        return Version(major_, 0, 0);
    throw std::invalid_argument("Invalid truncation level: " + std::string(level));
}

Version Version::coerce(std::string_view version_string) {
    std::size_t pos = 0;
    auto consume_num = [&]() -> std::string_view {
        auto s = pos;
        while (pos < version_string.size() && detail::is_digit(version_string[pos])) ++pos;
        return version_string.substr(s, pos - s);
    };

    auto num1 = consume_num();
    if (num1.empty())
        throw std::invalid_argument("Version string lacks a numerical component: " + std::string(version_string));

    std::string version(num1);
    if (pos < version_string.size() && version_string[pos] == '.') {
        ++pos;
        auto num2 = consume_num();
        if (!num2.empty()) {
            version += "." + std::string(num2);
            if (pos < version_string.size() && version_string[pos] == '.') {
                ++pos;
                auto num3 = consume_num();
                if (!num3.empty())
                    version += "." + std::string(num3);
            }
        }
    }

    // Fill missing components with .0
        while (std::count(version.begin(), version.end(), '.') < 2)
            version += ".0";

    auto parts = detail::split(version, '.');
    for (auto& p : parts) p = detail::lstrip_zeros(p);
    version = detail::join(parts, ".");

    auto end_pos = pos;
    if (end_pos == version_string.size())
        return Version(version);

    std::string rest(version_string.substr(end_pos));
    std::string cleaned;
    cleaned.reserve(rest.size());
    for (char c : rest) {
        if (detail::is_alnum_or_hyphen(c) || c == '+' || c == '.')
            cleaned += c;
        else
            cleaned += '-';
    }
    rest = cleaned;

    std::string prerelease_str, build_str;
    if (rest[0] == '+') {
        build_str = rest.substr(1);
    } else if (rest[0] == '.') {
        build_str = rest.substr(1);
    } else if (rest[0] == '-') {
        rest = rest.substr(1);
        auto plus_pos = rest.find('+');
        if (plus_pos != std::string::npos) {
            prerelease_str = rest.substr(0, plus_pos);
            build_str = rest.substr(plus_pos + 1);
        } else {
            prerelease_str = rest;
        }
    } else {
        auto plus_pos = rest.find('+');
        if (plus_pos != std::string::npos) {
            prerelease_str = rest.substr(0, plus_pos);
            build_str = rest.substr(plus_pos + 1);
        } else {
            prerelease_str = rest;
        }
    }

    // Replace extra '+' in build with '.'
    for (auto& c : build_str) {
        if (c == '+') c = '.';
    }

    if (!prerelease_str.empty())
        version += "-" + prerelease_str;
    if (!build_str.empty())
        version += "+" + build_str;

    return Version(version);
}

std::tuple<int, int, int,
           std::vector<std::string>,
           std::vector<std::string>>
Version::parse(std::string_view version_string) {
    if (version_string.empty())
        throw std::invalid_argument("Invalid empty version string");

    auto vp = detail::parse_version_parts(version_string);

    int mj = detail::parse_int(vp.major_s);
    int mn = detail::parse_int(vp.minor_s);
    int pa = detail::parse_int(vp.patch_s);

    std::vector<std::string> prerelease;
    if (vp.has_prerelease && !vp.prerelease_s.empty()) {
        prerelease = detail::split(vp.prerelease_s, '.');
        validate_identifiers(prerelease, false);
    }

    std::vector<std::string> build;
    if (vp.has_build && !vp.build_s.empty()) {
        build = detail::split(vp.build_s, '.');
        validate_identifiers(build, true);
    }

    return {mj, mn, pa, std::move(prerelease), std::move(build)};
}

bool Version::validate(std::string_view version_string) {
    try {
        std::ignore = parse(version_string);
        return true;
    } catch (...) {
        return false;
    }
}

void Version::rebuild_keys() {
    auto prerelease_key = build_prerelease_key();
    auto build_key = build_build_key();
    cmp_key_ = {major_, minor_, patch_, prerelease_key};
    sort_key_ = {major_, minor_, patch_, prerelease_key, build_key};
}

std::vector<detail::Identifier> Version::build_prerelease_key() const {
    if (!prerelease_.empty()) {
        std::vector<detail::Identifier> key;
        key.reserve(prerelease_.size());
        for (auto& p : prerelease_)
            key.push_back(detail::make_identifier(p));
        return key;
    }
    return {detail::MaxIdentifier{}};
}

std::vector<detail::Identifier> Version::build_build_key() const {
    std::vector<detail::Identifier> key;
    for (auto& b : build_)
            key.push_back(detail::make_identifier(b));
    return key;
}

/*static*/ void Version::validate_identifiers(const std::vector<std::string>& ids, bool allow_leading_zeroes) {
    for (auto& item : ids) {
        if (item.empty())
            throw std::invalid_argument("Invalid empty identifier in: " + detail::join(ids, "."));
        if (!allow_leading_zeroes && item.size() > 1 && item[0] == '0' && detail::is_digit_string(item))
            throw std::invalid_argument("Invalid leading zero in identifier: " + item);
    }
}

void Version::validate_kwargs() const {
    validate_identifiers(prerelease_, false);
    validate_identifiers(build_, true);
}

// ============================================================================
// Clause implementations
// ============================================================================

// --- Never ---
bool Never::match(const Version&) const { return false; }
bool Never::operator==(const Clause& o) const { return dynamic_cast<const Never*>(&o) != nullptr; }
std::size_t Never::hash_value() const { return 0xDEAD; }

// --- Always ---
bool Always::match(const Version&) const { return true; }
bool Always::operator==(const Clause& o) const { return dynamic_cast<const Always*>(&o) != nullptr; }
std::size_t Always::hash_value() const { return 0xBEEF; }

// --- Range ---
Range::Range(Op op, Version target, PrePolicy pp, BuildPolicy bp)
    : op(op), target(std::move(target)), pre_policy(pp),
      build_policy(!this->target.build().empty() ? BuildPolicy::STRICT : bp)
{
    if (!this->target.build().empty() && op != Op::EQ && op != Op::NEQ)
        throw std::invalid_argument("Build numbers have no ordering.");
}

bool Range::match(const Version& version) const {
    Version v = version;
    if (build_policy != BuildPolicy::STRICT)
        v = v.truncate("prerelease");

    if (!v.prerelease().empty()) {
        bool same_patch = target.truncate() == v.truncate();
        if (pre_policy == PrePolicy::SAME_PATCH && !same_patch)
            return false;
    }

    switch (op) {
    case Op::EQ:
        if (build_policy == BuildPolicy::STRICT) {
            return target.truncate("prerelease") == v.truncate("prerelease")
                && v.build() == target.build();
        }
        return v == target;
    case Op::GT: return v > target;
    case Op::GTE: return v >= target;
    case Op::LT:
        if (!v.prerelease().empty()
            && pre_policy == PrePolicy::NATURAL
            && v.truncate() == target.truncate()
            && target.prerelease().empty())
            return false;
        return v < target;
    case Op::LTE: return v <= target;
    case Op::NEQ:
        if (build_policy == BuildPolicy::STRICT) {
            return !(target.truncate("prerelease") == v.truncate("prerelease")
                     && v.build() == target.build());
        }
        if (!v.prerelease().empty()
            && pre_policy == PrePolicy::NATURAL
            && v.truncate() == target.truncate()
            && target.prerelease().empty())
            return false;
        return v != target;
    }
    return false;
}

bool Range::operator==(const Clause& o) const {
    if (auto* r = dynamic_cast<const Range*>(&o))
        return op == r->op && target == r->target && pre_policy == r->pre_policy;
    return false;
}

std::size_t Range::hash_value() const {
    std::size_t h = std::hash<int>{}(static_cast<int>(op));
    h ^= target.hash() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// --- AllOf ---
AllOf::AllOf(std::vector<ClausePtr> c) : clauses(std::move(c)) {}
AllOf::AllOf(std::initializer_list<ClausePtr> c) : clauses(c) {}

bool AllOf::match(const Version& version) const {
    return std::all_of(clauses.begin(), clauses.end(), [&](auto& c) { return c->match(version); });
}

bool AllOf::operator==(const Clause& o) const {
    if (auto* a = dynamic_cast<const AllOf*>(&o)) {
        if (clauses.size() != a->clauses.size()) return false;
        for (auto& c : clauses) {
            bool found = false;
            for (auto& oc : a->clauses) if (*c == *oc) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }
    return false;
}

std::size_t AllOf::hash_value() const {
    std::size_t h = 0xA110F;
    for (auto& c : clauses) h ^= c->hash_value() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// --- AnyOf ---
AnyOf::AnyOf(std::vector<ClausePtr> c) : clauses(std::move(c)) {}
AnyOf::AnyOf(std::initializer_list<ClausePtr> c) : clauses(c) {}

bool AnyOf::match(const Version& version) const {
    return std::any_of(clauses.begin(), clauses.end(), [&](auto& c) { return c->match(version); });
}

bool AnyOf::operator==(const Clause& o) const {
    if (auto* a = dynamic_cast<const AnyOf*>(&o)) {
        if (clauses.size() != a->clauses.size()) return false;
        for (auto& c : clauses) {
            bool found = false;
            for (auto& oc : a->clauses) if (*c == *oc) { found = true; break; }
            if (!found) return false;
        }
        return true;
    }
    return false;
}

std::size_t AnyOf::hash_value() const {
    std::size_t h = 0xA0F0F;
    for (auto& c : clauses) h ^= c->hash_value() + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
}

// --- Clause::and_with / or_with ---
ClausePtr Clause::and_with(ClausePtr other) const {
    if (dynamic_cast<const Always*>(this)) return other;
    if (dynamic_cast<const Always*>(other.get())) return const_cast<Clause*>(this)->shared_from_this();
    if (dynamic_cast<const Never*>(this)) return const_cast<Clause*>(this)->shared_from_this();
    if (dynamic_cast<const Never*>(other.get())) return other;

    std::vector<ClausePtr> merged;
    if (auto* a = dynamic_cast<const AllOf*>(this)) merged = a->clauses;
    else merged.push_back(const_cast<Clause*>(this)->shared_from_this());

    if (auto* a = dynamic_cast<const AllOf*>(other.get()))
        merged.insert(merged.end(), a->clauses.begin(), a->clauses.end());
    else
        merged.push_back(other);

    return std::make_shared<AllOf>(std::move(merged));
}

ClausePtr Clause::or_with(ClausePtr other) const {
    if (dynamic_cast<const Never*>(this)) return other;
    if (dynamic_cast<const Never*>(other.get())) return const_cast<Clause*>(this)->shared_from_this();
    if (dynamic_cast<const Always*>(this)) return const_cast<Clause*>(this)->shared_from_this();
    if (dynamic_cast<const Always*>(other.get())) return other;

    std::vector<ClausePtr> merged;
    if (auto* a = dynamic_cast<const AnyOf*>(this)) merged = a->clauses;
    else merged.push_back(const_cast<Clause*>(this)->shared_from_this());

    if (auto* a = dynamic_cast<const AnyOf*>(other.get()))
        merged.insert(merged.end(), a->clauses.begin(), a->clauses.end());
    else
        merged.push_back(other);

    return std::make_shared<AnyOf>(std::move(merged));
}

// --- Factory helpers ---
ClausePtr make_never() { return std::make_shared<Never>(); }
ClausePtr make_always() { return std::make_shared<Always>(); }
ClausePtr make_range(Range::Op op, Version target, Range::PrePolicy pp, Range::BuildPolicy bp) {
    return std::make_shared<Range>(op, std::move(target), pp, bp);
}

// ============================================================================
// BaseSpec implementation
// ============================================================================
BaseSpec::BaseSpec(std::string expr, ClausePtr c) : expression(std::move(expr)), clause(std::move(c)) {}

bool BaseSpec::match(const Version& v) const { return clause->match(v); }

std::vector<Version> BaseSpec::filter(const std::vector<Version>& versions) const {
    std::vector<Version> result;
    for (auto& v : versions)
        if (match(v)) result.push_back(v);
    return result;
}

std::optional<Version> BaseSpec::select(const std::vector<Version>& versions) const {
    auto filtered = filter(versions);
    if (filtered.empty()) return std::nullopt;
    return *std::max_element(filtered.begin(), filtered.end());
}

bool BaseSpec::contains(const Version& v) const { return match(v); }
bool BaseSpec::operator==(const BaseSpec& o) const { return *clause == *o.clause; }
std::size_t BaseSpec::hash() const { return clause->hash_value(); }
const std::string& BaseSpec::str() const { return expression; }
std::ostream& operator<<(std::ostream& os, const BaseSpec& s) { return os << s.expression; }

// ============================================================================
// SimpleSpec implementation
// ============================================================================
SimpleSpec::SimpleSpec(std::string_view expression) {
    this->expression = std::string(expression);
    this->clause = parse_expression(expression);
}

/*static*/ ClausePtr SimpleSpec::parse_expression(std::string_view expression) {
    auto blocks = detail::split(expression, ',');
    ClausePtr cl = make_always();
    for (auto& block : blocks)
        cl = cl->and_with(parse_block(block));
    return cl;
}

/*static*/ ClausePtr SimpleSpec::parse_block(std::string_view expr) {
    auto sp = detail::parse_spec_block(expr);

    std::string prefix(sp.prefix);
    if (prefix == "=" || prefix.empty()) prefix = "==";

    bool major_empty = detail::is_wildcard(sp.major_s);
    bool minor_empty = !sp.has_minor || detail::is_wildcard(sp.minor_s);
    bool patch_empty = !sp.has_patch || detail::is_wildcard(sp.patch_s);

    std::optional<int> major_v = major_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.major_s));
    std::optional<int> minor_v = minor_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.minor_s));
    std::optional<int> patch_v = patch_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.patch_s));

    std::string prerel(sp.prerelease_s);
    std::string build_str(sp.build_s);
    bool has_prerel = sp.has_prerelease;
    bool has_build = sp.has_build;

    Version target;
    if (!major_v.has_value()) {
        target = Version(0, 0, 0);
        if (prefix != "==" && prefix != ">=")
            throw std::invalid_argument("Invalid simple spec: " + std::string(expr));
    } else if (!minor_v.has_value()) {
        target = Version(*major_v, 0, 0);
    } else if (!patch_v.has_value()) {
        target = Version(*major_v, *minor_v, 0);
    } else {
        target = Version(*major_v, *minor_v, *patch_v,
                         prerel.empty() ? std::vector<std::string>{} : detail::split(prerel, '.'),
                         has_build ? (build_str.empty() ? std::vector<std::string>{} : detail::split(build_str, '.'))
                                   : std::vector<std::string>{});
    }

    if ((!major_v || !minor_v || !patch_v) && (!prerel.empty() || has_build))
        throw std::invalid_argument("Invalid simple spec: " + std::string(expr));

    if (has_build && prefix != "==" && prefix != "!=")
        throw std::invalid_argument("Invalid simple spec: " + std::string(expr));

    if (prefix == "^") {
        Version high;
        if (target.major() > 0) high = target.next_major();
        else if (target.minor() > 0) high = target.next_minor();
        else high = target.next_patch();
        return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, high));
    }
    if (prefix == "~") {
        Version high;
        if (!minor_v.has_value()) high = target.next_major();
        else high = target.next_minor();
        return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, high));
    }
    if (prefix == "~=") {
        if (!minor_v.has_value() || !patch_v.has_value()) {
            Version high = !minor_v.has_value() ? target.next_major() : target.next_major();
            return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, high));
        }
        return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, target.next_minor()));
    }
    if (prefix == "==") {
        if (!major_v.has_value())
            return make_range(Range::Op::GTE, target);
        if (!minor_v.has_value())
            return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, target.next_major()));
        if (!patch_v.has_value())
            return make_range(Range::Op::GTE, target)->and_with(make_range(Range::Op::LT, target.next_minor()));
        if (has_build)
            return make_range(Range::Op::EQ, target, Range::PrePolicy::NATURAL, Range::BuildPolicy::STRICT);
        return make_range(Range::Op::EQ, target);
    }
    if (prefix == "!=") {
        if (has_build)
            return make_range(Range::Op::NEQ, target, Range::PrePolicy::NATURAL, Range::BuildPolicy::STRICT);
        if (has_prerel && prerel.empty())
            return make_range(Range::Op::NEQ, target, Range::PrePolicy::ALWAYS);
        return make_range(Range::Op::NEQ, target);
    }
    if (prefix == ">") {
        if (!major_v.has_value()) throw std::invalid_argument("Invalid simple spec: " + std::string(expr));
        if (!minor_v.has_value()) return make_range(Range::Op::GTE, target.next_major());
        if (!patch_v.has_value()) return make_range(Range::Op::GTE, target.next_minor());
        return make_range(Range::Op::GT, target);
    }
    if (prefix == ">=") {
        return make_range(Range::Op::GTE, target);
    }
    if (prefix == "<") {
        if (!major_v.has_value()) throw std::invalid_argument("Invalid simple spec: " + std::string(expr));
        if (has_prerel && prerel.empty())
            return make_range(Range::Op::LT, target, Range::PrePolicy::ALWAYS);
        return make_range(Range::Op::LT, target);
    }
    if (prefix == "<=") {
        if (!major_v.has_value()) throw std::invalid_argument("Invalid simple spec: " + std::string(expr));
        if (!minor_v.has_value()) return make_range(Range::Op::LT, target.next_major());
        if (!patch_v.has_value()) return make_range(Range::Op::LT, target.next_minor());
        return make_range(Range::Op::LTE, target);
    }

    throw std::invalid_argument("Invalid simple spec prefix: " + prefix);
}

// ============================================================================
// NpmSpec implementation
// ============================================================================
NpmSpec::NpmSpec(std::string_view expression) {
    this->expression = std::string(expression);
    this->clause = parse_expression(expression);
}

/*static*/ ClausePtr NpmSpec::npm_range(Range::Op op, Version target) {
    return make_range(op, std::move(target), Range::PrePolicy::SAME_PATCH);
}

/*static*/ ClausePtr NpmSpec::parse_expression(std::string_view expression) {
    ClausePtr result = make_never();
    auto groups = split_joiner(expression);
    for (auto& group : groups) {
        auto trimmed = detail::trim(group);
        if (trimmed.empty()) trimmed = ">=0.0.0";

        std::vector<ClausePtr> subclauses;

        // Hyphen range.
        auto hyphen_pos = trimmed.find(" - ");
        if (hyphen_pos != std::string_view::npos) {
            auto low_s = trimmed.substr(0, hyphen_pos);
            auto high_s = trimmed.substr(hyphen_pos + 3);
            auto low_clauses = parse_simple(std::string(">=") + std::string(low_s));
            auto high_clauses = parse_simple(std::string("<=") + std::string(high_s));
            subclauses.insert(subclauses.end(), low_clauses.begin(), low_clauses.end());
            subclauses.insert(subclauses.end(), high_clauses.begin(), high_clauses.end());
        } else {
            // Split on whitespace.
            std::vector<std::string_view> blocks;
            std::size_t pos = 0;
            while (pos < trimmed.size()) {
                while (pos < trimmed.size() && trimmed[pos] == ' ') ++pos;
                if (pos >= trimmed.size()) break;
                auto start = pos;
                while (pos < trimmed.size() && trimmed[pos] != ' ') ++pos;
                blocks.push_back(trimmed.substr(start, pos - start));
            }
            for (auto& block : blocks) {
                auto cls = parse_simple(block);
                subclauses.insert(subclauses.end(), cls.begin(), cls.end());
            }
        }

        // Handle prerelease clauses per NPM spec.
        std::vector<ClausePtr> prerelease_clauses;
        std::vector<ClausePtr> non_prerel_clauses;
        for (auto& cl : subclauses) {
            auto* range = dynamic_cast<Range*>(cl.get());
            if (range && !range->target.prerelease().empty()) {
                if (range->op == Range::Op::GT || range->op == Range::Op::GTE) {
                    prerelease_clauses.push_back(std::make_shared<Range>(
                        Range::Op::LT,
                        Version(range->target.major(),
                                range->target.minor(),
                                range->target.patch() + 1),
                        Range::PrePolicy::ALWAYS));
                } else if (range->op == Range::Op::LT || range->op == Range::Op::LTE) {
                    prerelease_clauses.push_back(std::make_shared<Range>(
                        Range::Op::GTE,
                        Version(range->target.major(),
                                range->target.minor(),
                                0),
                        Range::PrePolicy::ALWAYS));
                }
                prerelease_clauses.push_back(cl);
                non_prerel_clauses.push_back(npm_range(range->op, range->target.truncate()));
            } else {
                non_prerel_clauses.push_back(cl);
            }
        }
        if (!prerelease_clauses.empty()) {
            result = result->or_with(std::make_shared<AllOf>(std::move(prerelease_clauses)));
        }
        result = result->or_with(std::make_shared<AllOf>(std::move(non_prerel_clauses)));
    }
    return result;
}

/*static*/ std::vector<ClausePtr> NpmSpec::parse_simple(std::string_view simple) {
    auto sp = detail::parse_spec_block(simple, true);

    std::string prefix(sp.prefix);
    if (prefix.empty()) prefix = "=";

    bool major_empty = detail::is_wildcard(sp.major_s);
    bool minor_empty = !sp.has_minor || detail::is_wildcard(sp.minor_s);
    bool patch_empty = !sp.has_patch || detail::is_wildcard(sp.patch_s);

    std::optional<int> major_v = major_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.major_s));
    std::optional<int> minor_v = minor_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.minor_s));
    std::optional<int> patch_v = patch_empty ? std::nullopt : std::optional<int>(detail::parse_int(sp.patch_s));

    std::string prerel(sp.prerelease_s);
    std::string build_str(sp.build_s);
    bool has_prerel = sp.has_prerelease && !prerel.empty();
    bool has_build = sp.has_build && !build_str.empty();

    Version target;
    if (major_v && minor_v && patch_v) {
        target = Version(*major_v, *minor_v, *patch_v,
                         has_prerel ? detail::split(prerel, '.') : std::vector<std::string>{},
                         has_build ? detail::split(build_str, '.') : std::vector<std::string>{});
    } else {
        target = Version(major_v.value_or(0), minor_v.value_or(0), patch_v.value_or(0));
    }

    if ((!major_v || !minor_v || !patch_v) && (has_prerel || has_build))
        throw std::invalid_argument("Invalid NPM spec: " + std::string(simple));

    if (prefix == "^") {
        Version high;
        if (target.major() > 0) {
            high = target.truncate().next_major();
        } else if (target.minor() > 0) {
            high = target.truncate().next_minor();
        } else if (!minor_v.has_value()) {
            high = target.truncate().next_major();
        } else if (!patch_v.has_value()) {
            high = target.truncate().next_minor();
        } else {
            high = target.truncate().next_patch();
        }
        return {npm_range(Range::Op::GTE, target), npm_range(Range::Op::LT, high)};
    }
    if (prefix == "~") {
        Version high;
        if (!minor_v.has_value()) high = target.next_major();
        else high = target.next_minor();
        return {npm_range(Range::Op::GTE, target), npm_range(Range::Op::LT, high)};
    }
    if (prefix == "=") {
        if (!major_v.has_value())
            return {npm_range(Range::Op::GTE, target)};
        if (!minor_v.has_value())
            return {npm_range(Range::Op::GTE, target), npm_range(Range::Op::LT, target.next_major())};
        if (!patch_v.has_value())
            return {npm_range(Range::Op::GTE, target), npm_range(Range::Op::LT, target.next_minor())};
        return {npm_range(Range::Op::EQ, target)};
    }
    if (prefix == ">") {
        if (!minor_v.has_value()) return {npm_range(Range::Op::GTE, target.next_major())};
        if (!patch_v.has_value()) return {npm_range(Range::Op::GTE, target.next_minor())};
        return {npm_range(Range::Op::GT, target)};
    }
    if (prefix == ">=") {
        return {npm_range(Range::Op::GTE, target)};
    }
    if (prefix == "<") {
        return {npm_range(Range::Op::LT, target)};
    }
    if (prefix == "<=") {
        if (!minor_v.has_value()) return {npm_range(Range::Op::LT, target.next_major())};
        if (!patch_v.has_value()) return {npm_range(Range::Op::LT, target.next_minor())};
        return {npm_range(Range::Op::LTE, target)};
    }

    throw std::invalid_argument("Invalid NPM spec prefix: " + prefix);
}

/*static*/ std::vector<std::string_view> NpmSpec::split_joiner(std::string_view s) {
    std::vector<std::string_view> parts;
    std::size_t pos = 0;
    while (pos < s.size()) {
        auto found = s.find("||", pos);
        if (found == std::string_view::npos) {
            parts.push_back(s.substr(pos));
            break;
        }
        parts.push_back(s.substr(pos, found - pos));
        pos = found + 2;
    }
    return parts;
}

// ============================================================================
// Free functions
// ============================================================================
/*extern*/ std::weak_ordering compare(std::string_view v1s, std::string_view v2s) {
    Version v1{v1s};
    Version v2{v2s};
    std::weak_ordering ord = v1 <=> v2;
    if((v1 == v2) != (ord == std::weak_ordering::equivalent))
        throw std::logic_error("Versions differ only in build metadata; no ordering defined.");
    return Version(v1) <=> Version(v2);
}

/*extern*/ bool match(std::string_view spec, std::string_view version) {
    return SimpleSpec(spec).match(Version(version));
}

/*extern*/ bool validate(std::string_view version_string) {
    return Version::validate(version_string);
}

} // namespace semver