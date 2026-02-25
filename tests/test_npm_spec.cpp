// test_npm_spec.cpp — Comprehensive NPM semver specification tests
// Tests derived from the node-semver (https://github.com/npm/node-semver) spec
// and the npm documentation (https://docs.npmjs.com/cli/v6/using-npm/semver).
//
// Covers: primitive comparators, hyphen ranges, X-ranges, tilde ranges,
// caret ranges, prerelease semantics, union (||), intersection (whitespace),
// build metadata, filter/select, and edge cases.

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>

#include "semver/semver.hpp"

using semver::Version;
using semver::NpmSpec;

// =========================================================================
// Helper: table-driven match / reject
// =========================================================================
struct NpmCase {
    std::string spec;
    std::vector<std::string> matching;
    std::vector<std::string> failing;
};

static void run_npm_cases(const std::vector<NpmCase>& cases) {
    for (auto& [spec_text, matching, failing] : cases) {
        auto spec = NpmSpec(spec_text);
        for (auto& ver : matching) {
            INFO("NpmSpec(\"" << spec_text << "\") should MATCH \"" << ver << "\"");
            REQUIRE(spec.match(Version(ver)));
        }
        for (auto& ver : failing) {
            INFO("NpmSpec(\"" << spec_text << "\") should REJECT \"" << ver << "\"");
            REQUIRE_FALSE(spec.match(Version(ver)));
        }
    }
}

// =========================================================================
// 1. Primitive comparators: =, >, >=, <, <=
// =========================================================================
TEST_CASE("NpmSpec: primitive — equality (= / bare)", "[npm][primitive]") {
    run_npm_cases({
        // Bare version means exact equality
        {"=1.2.3",
            {"1.2.3"},
            {"1.2.4", "1.2.2", "1.3.0", "2.0.0", "0.0.0"}},
        {"1.2.3",
            {"1.2.3"},
            {"1.2.4", "1.2.2"}},
    });
}

TEST_CASE("NpmSpec: primitive — greater than", "[npm][primitive]") {
    run_npm_cases({
        {">1.2.3",
            {"1.2.4", "1.3.0", "2.0.0", "99.0.0"},
            {"1.2.3", "1.2.2", "0.0.0"}},
        {">0.0.0",
            {"0.0.1", "1.0.0"},
            {"0.0.0"}},
    });
}

TEST_CASE("NpmSpec: primitive — greater than or equal", "[npm][primitive]") {
    run_npm_cases({
        {">=1.2.7",
            {"1.2.7", "1.2.8", "1.3.9", "2.5.3"},
            {"1.2.6", "1.1.0", "0.0.0"}},
        {">=0.0.0",
            {"0.0.0", "0.0.1", "999.999.999"},
            {}},
    });
}

TEST_CASE("NpmSpec: primitive — less than", "[npm][primitive]") {
    run_npm_cases({
        {"<1.2.3",
            {"1.2.2", "1.0.0", "0.0.0"},
            {"1.2.3", "1.2.4", "2.0.0"}},
        {"<0.0.1",
            {"0.0.0"},
            {"0.0.1", "1.0.0"}},
    });
}

TEST_CASE("NpmSpec: primitive — less than or equal", "[npm][primitive]") {
    run_npm_cases({
        {"<=1.2.3",
            {"1.2.3", "1.2.2", "0.0.0"},
            {"1.2.4", "2.0.0"}},
    });
}

// =========================================================================
// 2. Comparator intersection (whitespace-separated AND)
// =========================================================================
TEST_CASE("NpmSpec: comparator intersection (AND)", "[npm][intersection]") {
    run_npm_cases({
        {">=1.2.7 <1.3.0",
            {"1.2.7", "1.2.8", "1.2.99"},
            {"1.2.6", "1.3.0", "1.1.0"}},
        {">1.0.0 <2.0.0",
            {"1.0.1", "1.5.0", "1.99.99"},
            {"1.0.0", "2.0.0", "0.9.9"}},
        {">=1.0.0 <=1.0.0",
            {"1.0.0"},
            {"0.9.9", "1.0.1"}},
        {">=0.0.0 <0.0.0",
            {},
            {"0.0.0", "1.0.0"}},
    });
}

// =========================================================================
// 3. Union (||)
// =========================================================================
TEST_CASE("NpmSpec: union with ||", "[npm][union]") {
    run_npm_cases({
        {"1.2.7 || >=1.2.9 <2.0.0",
            {"1.2.7", "1.2.9", "1.4.6"},
            {"1.2.8", "2.0.0"}},
        {"<1.0.0 || >2.0.0",
            {"0.0.0", "0.9.9", "2.0.1", "3.0.0"},
            {"1.0.0", "1.5.0", "2.0.0"}},
        {"1.0.0 || 2.0.0 || 3.0.0",
            {"1.0.0", "2.0.0", "3.0.0"},
            {"1.0.1", "2.0.1", "0.0.0", "4.0.0"}},
        // Empty left side of || is >=0.0.0 (match-all)
        {"|| 1.0.0",
            {"0.0.0", "1.0.0", "99.0.0"},
            {}},
    });
}

// =========================================================================
// 4. Hyphen ranges:  A - B
// =========================================================================
TEST_CASE("NpmSpec: hyphen ranges — full versions", "[npm][hyphen]") {
    // 1.2.3 - 2.3.4  :=  >=1.2.3 <=2.3.4
    run_npm_cases({
        {"1.2.3 - 2.3.4",
            {"1.2.3", "1.5.0", "2.0.0", "2.3.4"},
            {"1.2.2", "2.3.5", "0.0.0", "3.0.0"}},
    });
}

TEST_CASE("NpmSpec: hyphen ranges — partial lower bound", "[npm][hyphen]") {
    // 1.2 - 2.3.4  :=  >=1.2.0 <=2.3.4
    run_npm_cases({
        {"1.2 - 2.3.4",
            {"1.2.0", "1.2.1", "2.3.4"},
            {"1.1.9", "2.3.5"}},
    });
}

TEST_CASE("NpmSpec: hyphen ranges — partial upper bound (minor)", "[npm][hyphen]") {
    // 1.2.3 - 2.3  :=  >=1.2.3 <2.4.0
    run_npm_cases({
        {"1.2.3 - 2.3",
            {"1.2.3", "2.3.0", "2.3.99"},
            {"1.2.2", "2.4.0", "3.0.0"}},
    });
}

TEST_CASE("NpmSpec: hyphen ranges — partial upper bound (major)", "[npm][hyphen]") {
    // 1.2.3 - 2  :=  >=1.2.3 <3.0.0
    run_npm_cases({
        {"1.2.3 - 2",
            {"1.2.3", "2.0.0", "2.99.99"},
            {"1.2.2", "3.0.0"}},
    });
}

TEST_CASE("NpmSpec: hyphen ranges — both partial", "[npm][hyphen]") {
    // 1 - 2  :=  >=1.0.0 <3.0.0
    run_npm_cases({
        {"1 - 2",
            {"1.0.0", "1.99.99", "2.0.0", "2.99.99"},
            {"0.99.99", "3.0.0"}},
    });
}

// =========================================================================
// 5. X-Ranges: *, x, X, and missing components
// =========================================================================
TEST_CASE("NpmSpec: X-range — star (match all)", "[npm][xrange]") {
    run_npm_cases({
        {"*",
            {"0.0.0", "1.2.3", "99.99.99"},
            {}},
    });
}

TEST_CASE("NpmSpec: X-range — empty string (match all)", "[npm][xrange]") {
    run_npm_cases({
        {"",
            {"0.0.0", "1.0.0", "99.99.99"},
            {}},
    });
}

TEST_CASE("NpmSpec: X-range — major only", "[npm][xrange]") {
    // 1  :=  1.x.x  :=  >=1.0.0 <2.0.0
    run_npm_cases({
        {"1",
            {"1.0.0", "1.2.3", "1.99.99"},
            {"0.99.99", "2.0.0"}},
        {"0",
            {"0.0.0", "0.99.99"},
            {"1.0.0"}},
    });
}

TEST_CASE("NpmSpec: X-range — major.minor", "[npm][xrange]") {
    // 1.2  :=  1.2.x  :=  >=1.2.0 <1.3.0
    run_npm_cases({
        {"1.2",
            {"1.2.0", "1.2.99"},
            {"1.1.99", "1.3.0", "2.0.0"}},
    });
}

TEST_CASE("NpmSpec: X-range — explicit x/X/*", "[npm][xrange]") {
    run_npm_cases({
        {"1.x",
            {"1.0.0", "1.99.99"},
            {"0.99.99", "2.0.0"}},
        {"1.X",
            {"1.0.0", "1.5.5"},
            {"2.0.0"}},
        {"1.*",
            {"1.0.0", "1.99.0"},
            {"2.0.0"}},
        {"1.2.x",
            {"1.2.0", "1.2.99"},
            {"1.3.0", "1.1.99"}},
        {"1.2.X",
            {"1.2.0", "1.2.5"},
            {"1.3.0"}},
        {"1.2.*",
            {"1.2.0", "1.2.99"},
            {"1.3.0"}},
        {"x",
            {"0.0.0", "99.99.99"},
            {}},
        {"X",
            {"0.0.0", "99.99.99"},
            {}},
    });
}

// =========================================================================
// 6. Tilde ranges
// =========================================================================
TEST_CASE("NpmSpec: tilde — full version", "[npm][tilde]") {
    // ~1.2.3  :=  >=1.2.3 <1.3.0
    run_npm_cases({
        {"~1.2.3",
            {"1.2.3", "1.2.4", "1.2.99"},
            {"1.2.2", "1.3.0", "2.0.0"}},
    });
}

TEST_CASE("NpmSpec: tilde — major.minor only", "[npm][tilde]") {
    // ~1.2  :=  >=1.2.0 <1.3.0
    run_npm_cases({
        {"~1.2",
            {"1.2.0", "1.2.99"},
            {"1.1.99", "1.3.0"}},
    });
}

TEST_CASE("NpmSpec: tilde — major only", "[npm][tilde]") {
    // ~1  :=  >=1.0.0 <2.0.0
    run_npm_cases({
        {"~1",
            {"1.0.0", "1.9.9"},
            {"0.99.99", "2.0.0"}},
    });
}

TEST_CASE("NpmSpec: tilde — zero major", "[npm][tilde]") {
    run_npm_cases({
        {"~0.2.3",
            {"0.2.3", "0.2.99"},
            {"0.2.2", "0.3.0"}},
        {"~0.2",
            {"0.2.0", "0.2.99"},
            {"0.1.99", "0.3.0"}},
        {"~0",
            {"0.0.0", "0.99.99"},
            {"1.0.0"}},
    });
}

TEST_CASE("NpmSpec: tilde — with prerelease", "[npm][tilde]") {
    // ~1.2.3-beta.2  :=  >=1.2.3-beta.2 <1.3.0
    // Prereleases in 1.2.3 that are >= beta.2 are allowed
    run_npm_cases({
        {"~1.2.3-beta.2",
            {"1.2.3-beta.2", "1.2.3-beta.4", "1.2.3", "1.2.4"},
            {"1.2.3-beta.1", "1.2.4-beta.2", "1.3.0"}},
    });
}

// =========================================================================
// 7. Caret ranges
// =========================================================================
TEST_CASE("NpmSpec: caret — major >= 1", "[npm][caret]") {
    // ^1.2.3  :=  >=1.2.3 <2.0.0
    run_npm_cases({
        {"^1.2.3",
            {"1.2.3", "1.2.4", "1.9.0", "1.99.99"},
            {"1.2.2", "2.0.0", "0.0.0"}},
        {"^1.0.0",
            {"1.0.0", "1.99.99"},
            {"0.99.99", "2.0.0"}},
    });
}

TEST_CASE("NpmSpec: caret — major 0, minor >= 1", "[npm][caret]") {
    // ^0.2.3  :=  >=0.2.3 <0.3.0
    run_npm_cases({
        {"^0.2.3",
            {"0.2.3", "0.2.4", "0.2.99"},
            {"0.2.2", "0.3.0", "1.0.0"}},
        {"^0.1.0",
            {"0.1.0", "0.1.99"},
            {"0.0.99", "0.2.0"}},
    });
}

TEST_CASE("NpmSpec: caret — major 0, minor 0", "[npm][caret]") {
    // ^0.0.3  :=  >=0.0.3 <0.0.4
    run_npm_cases({
        {"^0.0.3",
            {"0.0.3"},
            {"0.0.2", "0.0.4", "0.1.0"}},
        {"^0.0.0",
            {"0.0.0"},
            {"0.0.1", "0.1.0", "1.0.0"}},
    });
}

TEST_CASE("NpmSpec: caret — with prerelease", "[npm][caret]") {
    // ^1.2.3-beta.2  :=  >=1.2.3-beta.2 <2.0.0
    run_npm_cases({
        {"^1.2.3-beta.2",
            {"1.2.3-beta.2", "1.2.3-beta.4", "1.2.3", "1.9.0"},
            {"1.2.3-beta.1", "2.0.0"}},
        // ^0.0.3-beta  :=  >=0.0.3-beta <0.0.4
        {"^0.0.3-beta",
            {"0.0.3-beta", "0.0.3-pr.2", "0.0.3"},
            {"0.0.3-alpha", "0.0.4", "0.1.0"}},
    });
}

TEST_CASE("NpmSpec: caret — with missing patch (x-range)", "[npm][caret]") {
    // ^1.2.x  :=  >=1.2.0 <2.0.0
    run_npm_cases({
        {"^1.2.x",
            {"1.2.0", "1.99.99"},
            {"1.1.99", "2.0.0"}},
        // ^0.0.x  :=  >=0.0.0 <0.1.0
        {"^0.0.x",
            {"0.0.0", "0.0.99"},
            {"0.1.0"}},
        // ^0.0  :=  >=0.0.0 <0.1.0
        {"^0.0",
            {"0.0.0", "0.0.99"},
            {"0.1.0"}},
    });
}

TEST_CASE("NpmSpec: caret — with missing minor (x-range)", "[npm][caret]") {
    // ^1.x  :=  >=1.0.0 <2.0.0
    run_npm_cases({
        {"^1.x",
            {"1.0.0", "1.99.99"},
            {"0.99.99", "2.0.0"}},
        // ^0.x  :=  >=0.0.0 <1.0.0
        {"^0.x",
            {"0.0.0", "0.99.99"},
            {"1.0.0"}},
    });
}

// =========================================================================
// 8. Prerelease semantics (NPM SAME_PATCH policy)
// =========================================================================
TEST_CASE("NpmSpec: prerelease — same-patch allowed", "[npm][prerelease]") {
    // >1.2.3-alpha.3: prereleases on 1.2.3 only
    run_npm_cases({
        {">1.2.3-alpha.3",
            {"1.2.3-alpha.7", "1.2.3-beta.0", "1.2.3", "3.4.5"},
            {"1.2.3-alpha.3", "1.2.3-alpha.2", "3.4.5-alpha.9"}},
    });
}

TEST_CASE("NpmSpec: prerelease — gte with prerelease", "[npm][prerelease]") {
    run_npm_cases({
        {">=1.2.3-alpha.3",
            {"1.2.3-alpha.3", "1.2.3-alpha.7", "1.2.3", "3.4.5"},
            {"1.2.3-alpha.2", "3.4.5-alpha.9", "1.2.2"}},
    });
}

TEST_CASE("NpmSpec: prerelease — range between prereleases", "[npm][prerelease]") {
    // >1.2.3-alpha <1.2.3-beta
    run_npm_cases({
        {">1.2.3-alpha <1.2.3-beta",
            {"1.2.3-alpha.0", "1.2.3-alpha.1", "1.2.3-alpha.99"},
            {"1.2.3-alpha", "1.2.3", "1.2.3-beta", "1.2.3-beta.0", "1.2.3-bravo"}},
    });
}

TEST_CASE("NpmSpec: prerelease — releases always pass non-pre ranges", "[npm][prerelease]") {
    // A release version without prerelease tag always satisfies >X if X < release
    run_npm_cases({
        {">1.0.0",
            {"1.0.1", "2.0.0"},
            {"1.0.0", "1.0.1-alpha", "0.99.99"}},
    });
}

TEST_CASE("NpmSpec: prerelease — different tuple blocked", "[npm][prerelease]") {
    // Prereleases on different major.minor.patch are excluded
    run_npm_cases({
        {">=1.0.0 <2.0.0",
            {"1.0.0", "1.5.0", "1.99.99"},
            {"1.0.0-alpha", "1.5.0-beta", "2.0.0-rc.1"}},
        {"^1.0.0",
            {"1.0.0", "1.5.0"},
            {"1.0.0-alpha", "1.5.0-rc.1", "2.0.0-alpha"}},
    });
}

TEST_CASE("NpmSpec: prerelease — tilde with prerelease", "[npm][prerelease]") {
    run_npm_cases({
        {"~1.2.3-beta.2",
            {"1.2.3-beta.2", "1.2.3-beta.4", "1.2.3", "1.2.4"},
            {"1.2.3-beta.1", "1.2.4-beta.2", "1.3.0"}},
    });
}

// =========================================================================
// 9. Build metadata
// =========================================================================
TEST_CASE("NpmSpec: build metadata — ignored in ordering", "[npm][build]") {
    // Build metadata should be ignored for range matching (ordering)
    run_npm_cases({
        {">=1.0.0",
            {"1.0.0+build.42", "1.2.3+meta"},
            {}},
        {"1.2.3 - 2.3.4",
            {"2.3.4+b42", "1.2.3+anything"},
            {}},
    });
}

// =========================================================================
// 10. Expansion equivalences
//     Verify that shorthand syntax expands identically to long-form.
// =========================================================================
TEST_CASE("NpmSpec: expansion equivalence — hyphen ranges", "[npm][expand]") {
    struct Equiv { std::string shorthand; std::string expanded; };

    std::vector<std::string> probes = {
        "0.0.0", "0.0.1", "0.1.0", "0.2.3", "0.9.9",
        "1.0.0", "1.2.0", "1.2.3", "1.2.4", "1.3.0", "1.9.9",
        "2.0.0", "2.3.0", "2.3.4", "2.3.5", "2.4.0", "2.99.99",
        "3.0.0", "5.0.0",
    };

    std::vector<Equiv> equivs = {
        {"1.2.3 - 2.3.4", ">=1.2.3 <=2.3.4"},
        {"1.2 - 2.3.4",   ">=1.2.0 <=2.3.4"},
        {"1.2.3 - 2.3",   ">=1.2.3 <2.4.0"},
        {"1.2.3 - 2",     ">=1.2.3 <3"},
    };

    for (auto& [sh, ex] : equivs) {
        auto sh_spec = NpmSpec(sh);
        auto ex_spec = NpmSpec(ex);
        for (auto& probe : probes) {
            INFO("shorthand=\"" << sh << "\" expanded=\"" << ex << "\" probe=\"" << probe << "\"");
            REQUIRE(sh_spec.match(Version(probe)) == ex_spec.match(Version(probe)));
        }
    }
}

TEST_CASE("NpmSpec: expansion equivalence — X-ranges", "[npm][expand]") {
    struct Equiv { std::string shorthand; std::string expanded; };

    std::vector<std::string> probes = {
        "0.0.0", "0.1.0", "0.99.99",
        "1.0.0", "1.2.0", "1.2.3", "1.2.99", "1.3.0", "1.99.99",
        "2.0.0", "3.0.0",
    };

    std::vector<Equiv> equivs = {
        {"*",     ">=0.0.0"},
        {"1.x",   ">=1.0.0 <2.0.0"},
        {"1.2.x", ">=1.2.0 <1.3.0"},
        {"",      "*"},
        {"1",     ">=1.0.0 <2.0.0"},
        {"1.2",   ">=1.2.0 <1.3.0"},
    };

    for (auto& [sh, ex] : equivs) {
        auto sh_spec = NpmSpec(sh);
        auto ex_spec = NpmSpec(ex);
        for (auto& probe : probes) {
            INFO("shorthand=\"" << sh << "\" expanded=\"" << ex << "\" probe=\"" << probe << "\"");
            REQUIRE(sh_spec.match(Version(probe)) == ex_spec.match(Version(probe)));
        }
    }
}

TEST_CASE("NpmSpec: expansion equivalence — tilde ranges", "[npm][expand]") {
    struct Equiv { std::string shorthand; std::string expanded; };

    std::vector<std::string> probes = {
        "0.0.0", "0.0.1", "0.1.0", "0.2.0", "0.2.3", "0.2.99", "0.3.0",
        "0.99.99",
        "1.0.0", "1.2.0", "1.2.3", "1.2.99", "1.3.0", "1.9.9",
        "2.0.0", "3.0.0",
    };

    std::vector<Equiv> equivs = {
        {"~1.2.3", ">=1.2.3 <1.3.0"},
        {"~1.2",   ">=1.2.0 <1.3.0"},
        {"~1",     ">=1.0.0 <2.0.0"},
        {"~0.2.3", ">=0.2.3 <0.3.0"},
        {"~0.2",   ">=0.2.0 <0.3.0"},
        {"~0",     ">=0.0.0 <1.0.0"},
    };

    for (auto& [sh, ex] : equivs) {
        auto sh_spec = NpmSpec(sh);
        auto ex_spec = NpmSpec(ex);
        for (auto& probe : probes) {
            INFO("shorthand=\"" << sh << "\" expanded=\"" << ex << "\" probe=\"" << probe << "\"");
            REQUIRE(sh_spec.match(Version(probe)) == ex_spec.match(Version(probe)));
        }
    }
}

TEST_CASE("NpmSpec: expansion equivalence — tilde with prerelease", "[npm][expand]") {
    std::vector<std::string> probes = {
        "1.2.3-beta.1", "1.2.3-beta.2", "1.2.3-beta.4", "1.2.3",
        "1.2.4", "1.2.4-beta.2", "1.3.0",
    };

    auto sh = NpmSpec("~1.2.3-beta.2");
    auto ex = NpmSpec(">=1.2.3-beta.2 <1.3.0");
    for (auto& probe : probes) {
        INFO("tilde-prerel probe=\"" << probe << "\"");
        REQUIRE(sh.match(Version(probe)) == ex.match(Version(probe)));
    }
}

TEST_CASE("NpmSpec: expansion equivalence — caret ranges", "[npm][expand]") {
    struct Equiv { std::string shorthand; std::string expanded; };

    std::vector<std::string> probes = {
        "0.0.0", "0.0.1", "0.0.3", "0.0.4", "0.1.0", "0.2.3", "0.2.99",
        "0.3.0", "0.99.99",
        "1.0.0", "1.2.0", "1.2.3", "1.9.9",
        "2.0.0", "3.0.0",
    };

    std::vector<Equiv> equivs = {
        {"^1.2.3",  ">=1.2.3 <2.0.0"},
        {"^0.2.3",  ">=0.2.3 <0.3.0"},
        {"^0.0.3",  ">=0.0.3 <0.0.4"},
        {"^1.2.x",  ">=1.2.0 <2.0.0"},
        {"^0.0.x",  ">=0.0.0 <0.1.0"},
        {"^0.0",    ">=0.0.0 <0.1.0"},
        {"^1.x",    ">=1.0.0 <2.0.0"},
        {"^0.x",    ">=0.0.0 <1.0.0"},
    };

    for (auto& [sh, ex] : equivs) {
        auto sh_spec = NpmSpec(sh);
        auto ex_spec = NpmSpec(ex);
        for (auto& probe : probes) {
            INFO("shorthand=\"" << sh << "\" expanded=\"" << ex << "\" probe=\"" << probe << "\"");
            REQUIRE(sh_spec.match(Version(probe)) == ex_spec.match(Version(probe)));
        }
    }
}

TEST_CASE("NpmSpec: expansion equivalence — caret with prerelease", "[npm][expand]") {
    struct Equiv { std::string shorthand; std::string expanded; };

    std::vector<std::string> probes = {
        "0.0.3-alpha", "0.0.3-beta", "0.0.3-pr.2", "0.0.3", "0.0.4",
        "1.2.3-beta.1", "1.2.3-beta.2", "1.2.3-beta.4", "1.2.3", "1.9.0", "2.0.0",
    };

    std::vector<Equiv> equivs = {
        {"^1.2.3-beta.2", ">=1.2.3-beta.2 <2.0.0"},
        {"^0.0.3-beta",   ">=0.0.3-beta <0.0.4"},
    };

    for (auto& [sh, ex] : equivs) {
        auto sh_spec = NpmSpec(sh);
        auto ex_spec = NpmSpec(ex);
        for (auto& probe : probes) {
            INFO("shorthand=\"" << sh << "\" expanded=\"" << ex << "\" probe=\"" << probe << "\"");
            REQUIRE(sh_spec.match(Version(probe)) == ex_spec.match(Version(probe)));
        }
    }
}

// =========================================================================
// 11. Partial comparators with operators  (>1, <=2.3, etc.)
// =========================================================================
TEST_CASE("NpmSpec: partial comparators with operators", "[npm][partial]") {
    run_npm_cases({
        // >1  =>  >=2.0.0
        {">1",
            {"2.0.0", "3.0.0", "99.0.0"},
            {"1.0.0", "1.99.99", "0.0.0"}},
        // >1.2  =>  >=1.3.0
        {">1.2",
            {"1.3.0", "2.0.0"},
            {"1.2.0", "1.2.99", "1.0.0"}},
        // <=1  =>  <2.0.0
        {"<=1",
            {"0.0.0", "1.0.0", "1.99.99"},
            {"2.0.0"}},
        // <=1.2  =>  <1.3.0
        {"<=1.2",
            {"0.0.0", "1.2.0", "1.2.99"},
            {"1.3.0", "2.0.0"}},
        // <1  =>  <1.0.0
        {"<1",
            {"0.0.0", "0.99.99"},
            {"1.0.0", "2.0.0"}},
        // <1.2  =>  <1.2.0
        {"<1.2",
            {"0.0.0", "1.1.99"},
            {"1.2.0", "1.3.0"}},
        // >=1  =>  >=1.0.0
        {">=1",
            {"1.0.0", "99.0.0"},
            {"0.0.0", "0.99.99"}},
        // >=1.2  =>  >=1.2.0
        {">=1.2",
            {"1.2.0", "2.0.0"},
            {"1.1.99", "0.0.0"}},
    });
}

// =========================================================================
// 12. Complex combined expressions
// =========================================================================
TEST_CASE("NpmSpec: complex — caret with union", "[npm][complex]") {
    // Real-world-ish: ^1.2.0 || >=3.0.0-beta <3.0.1
    run_npm_cases({
        {"^1.2.0 || >=3.0.0-beta <3.0.1",
            {"1.2.0", "1.5.0", "1.99.99", "3.0.0-beta", "3.0.0-beta.2", "3.0.0"},
            {"1.1.99", "2.0.0", "3.0.1", "3.0.1-alpha"}},
    });
}

TEST_CASE("NpmSpec: complex — tilde union caret", "[npm][complex]") {
    run_npm_cases({
        {"~1.2.3 || ^0.5.0",
            {"1.2.3", "1.2.99", "0.5.0", "0.5.99"},
            {"1.3.0", "0.4.99", "0.6.0", "2.0.0"}},
    });
}

TEST_CASE("NpmSpec: complex — multiple || clauses", "[npm][complex]") {
    // 1.x || >=2.5.0 || 5.0.0 - 7.2.3
    run_npm_cases({
        {"1.x || >=2.5.0 || 5.0.0 - 7.2.3",
            {"1.0.0", "1.99.99", "2.5.0", "3.0.0", "5.0.0", "7.2.3"},
            {"0.0.0", "2.0.0", "2.4.99"}},
    });
}

TEST_CASE("NpmSpec: complex — intersection with gaps", "[npm][complex]") {
    // >=1.0.0 <1.5.0 || >=2.0.0 <2.5.0
    run_npm_cases({
        {">=1.0.0 <1.5.0 || >=2.0.0 <2.5.0",
            {"1.0.0", "1.4.99", "2.0.0", "2.4.99"},
            {"0.99.99", "1.5.0", "1.99.99", "2.5.0", "3.0.0"}},
    });
}

// =========================================================================
// 13. Edge cases
// =========================================================================
TEST_CASE("NpmSpec: edge — 0.0.0 boundaries", "[npm][edge]") {
    run_npm_cases({
        {">=0.0.0",
            {"0.0.0", "999.999.999"},
            {}},
        {">0.0.0",
            {"0.0.1"},
            {"0.0.0"}},
        {"<=0.0.0",
            {"0.0.0"},
            {"0.0.1"}},
    });
}

TEST_CASE("NpmSpec: edge — large version numbers", "[npm][edge]") {
    run_npm_cases({
        {">=100.200.300",
            {"100.200.300", "100.200.301", "999.999.999"},
            {"100.200.299", "0.0.0"}},
    });
}

TEST_CASE("NpmSpec: edge — caret ^0.0.0 pins exactly", "[npm][edge]") {
    // ^0.0.0  :=  >=0.0.0 <0.0.1
    run_npm_cases({
        {"^0.0.0",
            {"0.0.0"},
            {"0.0.1", "0.1.0", "1.0.0"}},
    });
}

TEST_CASE("NpmSpec: edge — tilde ~0.0.0", "[npm][edge]") {
    // ~0.0.0 => >=0.0.0 <0.1.0
    run_npm_cases({
        {"~0.0.0",
            {"0.0.0", "0.0.1", "0.0.99"},
            {"0.1.0", "1.0.0"}},
    });
}

TEST_CASE("NpmSpec: edge — multiple whitespace in expression", "[npm][edge]") {
    // Extra whitespace between comparators should be tolerated
    run_npm_cases({
        {">=1.0.0  <2.0.0",
            {"1.0.0", "1.5.0"},
            {"0.9.9", "2.0.0"}},
    });
}

// =========================================================================
// 14. filter() and select() with NpmSpec
// =========================================================================
TEST_CASE("NpmSpec: filter — basic", "[npm][filter]") {
    auto spec = NpmSpec(">=1.0.0 <2.0.0");
    std::vector<Version> versions = {
        Version("0.9.0"),
        Version("1.0.0"),
        Version("1.3.0"),
        Version("1.9.9"),
        Version("2.0.0"),
        Version("3.0.0"),
    };

    auto filtered = spec.filter(versions);
    REQUIRE(filtered.size() == 3);
    REQUIRE(filtered[0] == Version("1.0.0"));
    REQUIRE(filtered[1] == Version("1.3.0"));
    REQUIRE(filtered[2] == Version("1.9.9"));
}

TEST_CASE("NpmSpec: filter — empty result", "[npm][filter]") {
    auto spec = NpmSpec(">=5.0.0");
    std::vector<Version> versions = {
        Version("1.0.0"),
        Version("2.0.0"),
        Version("3.0.0"),
    };
    auto filtered = spec.filter(versions);
    REQUIRE(filtered.empty());
}

TEST_CASE("NpmSpec: select — picks highest match", "[npm][select]") {
    auto spec = NpmSpec("^1.0.0");
    std::vector<Version> versions = {
        Version("0.9.0"),
        Version("1.0.0"),
        Version("1.5.0"),
        Version("1.9.9"),
        Version("2.0.0"),
    };

    auto best = spec.select(versions);
    REQUIRE(best.has_value());
    REQUIRE(*best == Version("1.9.9"));
}

TEST_CASE("NpmSpec: select — no match returns nullopt", "[npm][select]") {
    auto spec = NpmSpec(">=10.0.0");
    std::vector<Version> versions = {
        Version("1.0.0"),
        Version("2.0.0"),
    };
    auto result = spec.select(versions);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("NpmSpec: select — empty input", "[npm][select]") {
    auto spec = NpmSpec("*");
    auto result = spec.select({});
    REQUIRE_FALSE(result.has_value());
}

// =========================================================================
// 15. contains() alias
// =========================================================================
TEST_CASE("NpmSpec: contains — matches same as match()", "[npm][contains]") {
    auto spec = NpmSpec("^1.0.0");
    REQUIRE(spec.contains(Version("1.5.0")));
    REQUIRE_FALSE(spec.contains(Version("2.0.0")));
}

// =========================================================================
// 16. Real-world patterns from package.json dependencies
// =========================================================================
TEST_CASE("NpmSpec: real-world patterns", "[npm][realworld]") {
    run_npm_cases({
        // Typical dependency ranges
        {"^16.8.0",  // React peer dep style
            {"16.8.0", "16.14.0", "16.99.99"},
            {"16.7.99", "17.0.0"}},
        {"~4.17.21",  // lodash style
            {"4.17.21", "4.17.22", "4.17.99"},
            {"4.17.20", "4.18.0"}},
        {">=3.0.0 <4.0.0 || >=4.1.0 <5.0.0",  // gap range
            {"3.0.0", "3.99.99", "4.1.0", "4.99.99"},
            {"2.99.99", "4.0.0", "4.0.99", "5.0.0"}},
        {"*",  // accept anything
            {"0.0.0", "1.0.0", "99.99.99"},
            {}},
        {">=1.0.0",  // lower bound only
            {"1.0.0", "999.0.0"},
            {"0.99.99"}},
    });
}

// =========================================================================
// 17. Numeric prerelease ordering
// =========================================================================
TEST_CASE("NpmSpec: numeric prerelease ordering", "[npm][prerelease][ordering]") {
    // Numeric identifiers sort numerically: alpha.3 < alpha.7 < alpha.10
    run_npm_cases({
        {">1.0.0-alpha.3",
            {"1.0.0-alpha.4", "1.0.0-alpha.7", "1.0.0-alpha.10", "1.0.0"},
            {"1.0.0-alpha.3", "1.0.0-alpha.2", "1.0.0-alpha.1"}},
    });
}

// =========================================================================
// 18. v-prefix stripping
// =========================================================================
TEST_CASE("NpmSpec: v-prefix is stripped from comparators", "[npm][vprefix]") {
    // Per the npm spec, a leading "v" is stripped from version strings
    // in comparators. This works with any operator.
    run_npm_cases({
        {"v1.2.3",
            {"1.2.3"},
            {"1.2.4", "1.2.2"}},
        {">=v1.0.0",
            {"1.0.0", "2.0.0"},
            {"0.99.99"}},
        {"~v1.2.3",
            {"1.2.3", "1.2.99"},
            {"1.3.0"}},
        {"^v0.2.3",
            {"0.2.3", "0.2.99"},
            {"0.3.0"}},
    });
}

// =========================================================================
// 19. Comparator set equivalence (1.x.x == 1.x == 1.*)
// =========================================================================
TEST_CASE("NpmSpec: x-range synonyms produce same results", "[npm][xrange][synonym]") {
    std::vector<std::string> probes = {
        "0.0.0", "0.99.99", "1.0.0", "1.5.5", "1.99.99", "2.0.0",
    };

    std::vector<std::string> synonyms = {"1.x.x", "1.x", "1.*", "1.X", "1"};

    auto ref = NpmSpec(">=1.0.0 <2.0.0");
    for (auto& syn : synonyms) {
        auto spec = NpmSpec(syn);
        for (auto& probe : probes) {
            INFO("synonym=\"" << syn << "\" probe=\"" << probe << "\"");
            REQUIRE(spec.match(Version(probe)) == ref.match(Version(probe)));
        }
    }
}