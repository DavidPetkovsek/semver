// test_npm_extras.cpp — Tests for npm_match, NpmSpec::min_version, NpmSpec::subset
// These NPM-inspired additions mirror node-semver's satisfies(), minVersion(),
// and subset() functions.

#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <string>
#include <vector>

#include "semver/semver.hpp"

using semver::Version;
using semver::NpmSpec;
using semver::SimpleSpec;

// =========================================================================
// 1. npm_match — free function
// =========================================================================
TEST_CASE("npm_match: basic matching", "[npm][npm_match]") {
    REQUIRE(semver::npm_match("^1.0.0", "1.5.0"));
    REQUIRE(semver::npm_match(">=1.2.7", "1.2.7"));
    REQUIRE(semver::npm_match(">=1.2.7 <1.3.0", "1.2.8"));
    REQUIRE(semver::npm_match("1.2.7 || >=1.2.9 <2.0.0", "1.4.6"));
    REQUIRE(semver::npm_match("*", "99.99.99"));
}

TEST_CASE("npm_match: rejections", "[npm][npm_match]") {
    REQUIRE_FALSE(semver::npm_match("^1.0.0", "2.0.0"));
    REQUIRE_FALSE(semver::npm_match(">=1.2.7 <1.3.0", "1.3.0"));
    REQUIRE_FALSE(semver::npm_match("1.2.7 || >=1.2.9 <2.0.0", "1.2.8"));
    REQUIRE_FALSE(semver::npm_match("^0.0.3", "0.0.4"));
}

TEST_CASE("npm_match: prerelease semantics", "[npm][npm_match]") {
    // Prerelease only matches when comparator targets same patch
    REQUIRE(semver::npm_match(">1.2.3-alpha.3", "1.2.3-alpha.7"));
    REQUIRE(semver::npm_match(">1.2.3-alpha.3", "3.4.5"));
    REQUIRE_FALSE(semver::npm_match(">1.2.3-alpha.3", "3.4.5-alpha.9"));
}

// =========================================================================
// 2. NpmSpec::min_version
// =========================================================================

// Table-driven: [range, expected_min]  (empty string means nullopt)
struct MinVersionCase {
    std::string range;
    std::string expected; // "" means nullopt (impossible range)
};

TEST_CASE("min_version: stars and wildcards", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"*",           "0.0.0"},
        {"* || >=2",    "0.0.0"},
        {">=2 || *",    "0.0.0"},
        {">2 || *",     "0.0.0"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto spec = NpmSpec(range);
        auto result = spec.min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: exact / equality", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"1.0.0",       "1.0.0"},
        {"=1.0.0",      "1.0.0"},
        {"1.0.x",       "1.0.0"},
        {"1.x",         "1.0.0"},
        {"1",           "1.0.0"},
        {"1.x.x",       "1.0.0"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: tilde ranges", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"~1.1.1",                   "1.1.1"},
        {"~1.1.1-beta",              "1.1.1-beta"},
        {"~1.1.1 || >=2",           "1.1.1"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: caret ranges", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"^1.1.1",                   "1.1.1"},
        {"^1.1.1-beta",              "1.1.1-beta"},
        {"^1.1.1 || >=2",           "1.1.1"},
        {"^2.16.2 ^2.16",           "2.16.2"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: hyphen ranges", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"1.1.1 - 1.8.0",           "1.1.1"},
        {"1.1 - 1.8.0",             "1.1.0"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: less-than ranges", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {"<2",                       "0.0.0"},
        {"<2 || >4",                "0.0.0"},
        {">4 || <2",                "0.0.0"},
        {"<=2 || >=4",              "0.0.0"},
        {">=4 || <=2",              "0.0.0"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: greater-than-or-equal", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {">=1.1.1 <2 || >=2.2.2 <2", "1.1.1"},
        {">=2.2.2 <2 || >=1.1.1 <2", "1.1.1"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: greater-than (not equal)", "[npm][min_version]") {
    std::vector<MinVersionCase> cases = {
        {">1.0.0",                   "1.0.1"},
        {">2 || >1.0.0",            "1.0.1"},
        {">1.0.0-0",                "1.0.0-0.0"},
        {">1.0.0-beta",             "1.0.0-beta.0"},
        {">2 || >1.0.0-0",          "1.0.0-0.0"},
        {">2 || >1.0.0-beta",       "1.0.0-beta.0"},
    };
    for (auto& [range, expected] : cases) {
        INFO("range=\"" << range << "\"");
        auto result = NpmSpec(range).min_version();
        REQUIRE(result.has_value());
        REQUIRE(result->to_string() == expected);
    }
}

TEST_CASE("min_version: prerelease less-than", "[npm][min_version]") {
    // <0.0.0-beta  →  the lowest possible pre is 0.0.0-0
    auto result = NpmSpec("<0.0.0-beta").min_version();
    REQUIRE(result.has_value());
    REQUIRE(result->to_string() == "0.0.0-0");
}

TEST_CASE("min_version: <0.0.1-beta starts at 0.0.0", "[npm][min_version]") {
    auto result = NpmSpec("<0.0.1-beta").min_version();
    REQUIRE(result.has_value());
    REQUIRE(result->to_string() == "0.0.0");
}

TEST_CASE("min_version: impossible range returns nullopt", "[npm][min_version]") {
    auto result = NpmSpec(">4 <3").min_version();
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("min_version: result always satisfies the range", "[npm][min_version]") {
    // Smoke test: for every range that has a min, it must actually match.
    std::vector<std::string> ranges = {
        "*", ">=1.0.0", "^1.2.3", "~0.2.3", ">0.0.0",
        "1.0.0 - 2.0.0", ">=1.0.0 <2.0.0 || >=3.0.0",
        "^0.0.1", "~1.1.1-beta",
    };
    for (auto& r : ranges) {
        INFO("range=\"" << r << "\"");
        auto spec = NpmSpec(r);
        auto mv = spec.min_version();
        if (mv.has_value()) {
            REQUIRE(spec.match(*mv));
        }
    }
}

// =========================================================================
// 3. NpmSpec::subset
// =========================================================================

struct SubsetCase {
    std::string sub;
    std::string dom;
    bool expected;
};

TEST_CASE("subset: exact versions", "[npm][subset]") {
    std::vector<SubsetCase> cases = {
        {"1.2.3",                    "1.2.3",           true},
        {"1.2.3",                    "1.x",             true},
        {"1.2.3",                    ">1.2.0",          true},
        {"1.2.3 2.3.4 || 2.3.4",    "3",               false},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        auto dom_spec = NpmSpec(dom);
        auto sub_spec = NpmSpec(sub);
        REQUIRE(dom_spec.subset(sub_spec) == expected);
    }
}

TEST_CASE("subset: wildcard / star is subset of star", "[npm][subset]") {
    std::vector<SubsetCase> cases = {
        {"*",       "*",        true},
        {"",        "*",        true},
        {"*",       "",         true},
        {"",        "",         true},
        {"1.2.3",   "*",        true},
        {"^1.2.3",  "*",        true},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        REQUIRE(NpmSpec(dom).subset(NpmSpec(sub)) == expected);
    }
}

TEST_CASE("subset: union ranges", "[npm][subset]") {
    std::vector<SubsetCase> cases = {
        {"1 || 2 || 3",             ">=1.0.0",          true},
        {"^2 || ^3 || ^4",          ">=1",              true},
        {"^2 || ^3 || ^4",          ">=2",              true},
        {"^2 || ^3 || ^4",          ">=3",              false},
        {"^2",                       "^2 || ^3 || ^4",  true},
        {"^3",                       "^2 || ^3 || ^4",  true},
        {"^4",                       "^2 || ^3 || ^4",  true},
        {"^1",                       "^2 || ^3 || ^4",  false},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        REQUIRE(NpmSpec(dom).subset(NpmSpec(sub)) == expected);
    }
}

TEST_CASE("subset: bound comparisons", "[npm][subset]") {
    std::vector<SubsetCase> cases = {
        {">=1.0.0",                  "1.0.0",           false},
        {">=1.0.0 <2.0.0",          "<2.0.0",          true},
        {">=1.0.0 <2.0.0",          ">0.0.0",          true},
        {">=1.0.0 <=1.0.0",         "1.0.0",           true},
        {">=1.0.0 <=1.0.0",         "2.0.0",           false},
        {"<2.0.0",                   ">=1.0.0 <2.0.0",  false},
        {">=1.0.0",                  ">=1.0.0 <2.0.0",  false},
        {">=1.0.0 <2.0.0",          ">=1.0.0",          true},
        {">=1.0.0 <2.0.0",          ">1.0.0",           false},
        {">=1.0.0 <=2.0.0",         "<2.0.0",           false},
        {">=1.0.0",                  "<1.0.0",           false},
        {"<=1.0.0",                  ">1.0.0",           false},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        REQUIRE(NpmSpec(dom).subset(NpmSpec(sub)) == expected);
    }
}

TEST_CASE("subset: null-set ranges are subsets of everything", "[npm][subset]") {
    // Impossible ranges (null sets) are subsets of anything.
    std::vector<SubsetCase> cases = {
        {">2 <1",                    "3",               true},
        {"1.2.3 1.2.4",             "1.2.3",           true}, // intersection is empty
        {"1.2.3 1.2.4",             "1.2.9",           true},
        {"<=1.0.0 >1.0.0",          ">1.0.0",          true},
        {"1.0.0 >1.0.0",            ">1.0.0",          true},
        {"1.0.0 <1.0.0",            ">1.0.0",          true},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        REQUIRE(NpmSpec(dom).subset(NpmSpec(sub)) == expected);
    }
}

TEST_CASE("subset: multiple same-direction bounds", "[npm][subset]") {
    std::vector<SubsetCase> cases = {
        {"<1 <2 <3",                "<4",               true},
        {"<3 <2 <1",                "<4",               true},
        {">1 >2 >3",                ">0",               true},
        {">3 >2 >1",                ">0",               true},
        {"<=1 <=2 <=3",             "<4",               true},
        {"<=3 <=2 <=1",             "<4",               true},
        {">=1 >=2 >=3",             ">0",               true},
        {">=3 >=2 >=1",             ">0",               true},
        {">=3 >=2 >=1",             ">=3 >=2 >=1",      true},
        {">2.0.0",                   ">=2.0.0",          true},
    };
    for (auto& [sub, dom, expected] : cases) {
        INFO("sub=\"" << sub << "\" dom=\"" << dom << "\"");
        REQUIRE(NpmSpec(dom).subset(NpmSpec(sub)) == expected);
    }
}

TEST_CASE("subset: self is always a subset of itself", "[npm][subset]") {
    std::vector<std::string> ranges = {
        "^1", ">=1.0.0 <2.0.0", "*", "~1.2.3", "1.0.0",
        "^2 || ^3 || ^4", ">1 <5",
    };
    for (auto& r : ranges) {
        INFO("range=\"" << r << "\"");
        auto spec = NpmSpec(r);
        REQUIRE(spec.subset(spec));
    }
}

TEST_CASE("subset: combined EQ and union", "[npm][subset]") {
    REQUIRE(NpmSpec("1.0.0 || 2.0.0").subset(
            NpmSpec(">=1.0.0 <=1.0.0 || 2.0.0")));
    REQUIRE(NpmSpec("1.0.0 || 2.0.0").subset(
            NpmSpec("<=1.0.0 >=1.0.0 || 2.0.0")));
}

// =========================================================================
// 4. SimpleSpec::min_version  (inherited from BaseSpec)
// =========================================================================

TEST_CASE("SimpleSpec::min_version: basic ranges", "[simple][min_version]") {
    REQUIRE(SimpleSpec(">=1.0.0").min_version() == Version("1.0.0"));
    REQUIRE(SimpleSpec(">=1.2.3,<2.0.0").min_version() == Version("1.2.3"));
    REQUIRE(SimpleSpec("*").min_version() == Version("0.0.0"));
    REQUIRE(SimpleSpec("^1.2.3").min_version() == Version("1.2.3"));
    REQUIRE(SimpleSpec("~0.2.3").min_version() == Version("0.2.3"));
}

TEST_CASE("SimpleSpec::min_version: greater-than", "[simple][min_version]") {
    auto result = SimpleSpec(">1.0.0").min_version();
    REQUIRE(result.has_value());
    REQUIRE(*result == Version("1.0.1"));
}

TEST_CASE("SimpleSpec::min_version: result satisfies the spec", "[simple][min_version]") {
    std::vector<std::string> specs = {
        ">=1.0.0", "^1.2.3", "~0.2.3", ">0.0.0", "*",
        ">=1.0.0,<2.0.0", "^0.0.1",
    };
    for (auto& s : specs) {
        INFO("spec=\"" << s << "\"");
        auto spec = SimpleSpec(s);
        auto mv = spec.min_version();
        if (mv.has_value()) {
            REQUIRE(spec.match(*mv));
        }
    }
}

// =========================================================================
// 5. SimpleSpec::subset  (inherited from BaseSpec)
// =========================================================================

TEST_CASE("SimpleSpec::subset: basic", "[simple][subset]") {
    REQUIRE(SimpleSpec(">=1.0.0").subset(SimpleSpec("^1.2.3")));
    REQUIRE_FALSE(SimpleSpec("^1.2.3").subset(SimpleSpec(">=1.0.0")));
}

TEST_CASE("SimpleSpec::subset: self is subset of self", "[simple][subset]") {
    std::vector<std::string> specs = {
        ">=1.0.0", "^1.2.3", "~0.2.3", "*", ">=1.0.0,<2.0.0",
    };
    for (auto& s : specs) {
        INFO("spec=\"" << s << "\"");
        auto spec = SimpleSpec(s);
        REQUIRE(spec.subset(spec));
    }
}

TEST_CASE("SimpleSpec::subset: wildcard contains everything", "[simple][subset]") {
    REQUIRE(SimpleSpec("*").subset(SimpleSpec("^1.0.0")));
    REQUIRE(SimpleSpec("*").subset(SimpleSpec(">=5.0.0")));
}

// =========================================================================
// 6. PrePolicy difference between SimpleSpec and NpmSpec
// =========================================================================

TEST_CASE("PrePolicy: SimpleSpec vs NpmSpec prerelease semantics differ", "[simple][npm][prepolicy]") {
    // SimpleSpec uses PrePolicy::NATURAL — a prerelease on a different
    // major.minor.patch tuple CAN match if it's within the numeric range.
    // NpmSpec uses PrePolicy::SAME_PATCH — a prerelease on a different
    // tuple is BLOCKED unless the comparator targets that same tuple.
    //
    // This is exactly why cross-spec subset comparison is disallowed.

    auto ver = Version("3.4.5-alpha.9");

    // NpmSpec: >1.2.3-alpha.3 blocks 3.4.5-alpha.9 (different patch tuple)
    auto npm = NpmSpec(">1.2.3-alpha.3");
    REQUIRE_FALSE(npm.match(ver));

    // SimpleSpec: >1.2.3-alpha.3 allows 3.4.5-alpha.9 (NATURAL policy
    // doesn't enforce same-patch)
    auto simple = SimpleSpec(">1.2.3-alpha.3");
    REQUIRE(simple.match(ver));

    // Both agree on same-patch prereleases
    auto same_patch = Version("1.2.3-alpha.7");
    REQUIRE(npm.match(same_patch));
    REQUIRE(simple.match(same_patch));

    // Both agree on plain releases above the range
    auto release = Version("3.4.5");
    REQUIRE(npm.match(release));
    REQUIRE(simple.match(release));
}