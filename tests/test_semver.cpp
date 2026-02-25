// test_semver.cpp — Catch2 unit tests for semver.hpp
// A faithful translation of the python-semanticversion test suite.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <set>
#include <string>
#include <vector>

#include "semver/semver.hpp"

using semver::Version;
using semver::SimpleSpec;
using semver::NpmSpec;
using Vs = std::vector<std::string>;

// Helper: check that constructing a Version from a string throws.
#define REQUIRE_INVALID_VERSION(str) REQUIRE_THROWS_AS(Version(str), std::invalid_argument)
#define REQUIRE_VALID_VERSION(str)   REQUIRE_NOTHROW(Version(str))

// ==========================================================================
// test_spec.py — FormatTests
// ==========================================================================

TEST_CASE("FormatTests: major_minor_patch", "[spec][format]") {
    // A normal version number MUST take the form X.Y.Z
    REQUIRE_INVALID_VERSION("1");
    REQUIRE_INVALID_VERSION("1.1");
    REQUIRE_VALID_VERSION("1.2.3");
    REQUIRE_INVALID_VERSION("1.2.3.4");

    // X, Y, Z are non-negative integers
    REQUIRE_INVALID_VERSION("1.2.A");
    REQUIRE_INVALID_VERSION("1.-2.3");

    auto v = Version("1.2.3");
    REQUIRE(v.major() == 1);
    REQUIRE(v.minor() == 2);
    REQUIRE(v.patch() == 3);

    // MUST NOT contain leading zeroes
    REQUIRE_INVALID_VERSION("1.2.01");
    REQUIRE_INVALID_VERSION("1.02.1");
    REQUIRE_INVALID_VERSION("01.2.1");

    auto v0 = Version("0.0.0");
    REQUIRE(v0.major() == 0);
    REQUIRE(v0.minor() == 0);
    REQUIRE(v0.patch() == 0);
}

TEST_CASE("FormatTests: prerelease", "[spec][format]") {
    // Space before hyphen is invalid.
    REQUIRE_INVALID_VERSION("1.2.3 -23");

    auto v = Version("1.2.3-23");
    REQUIRE(v.prerelease().value() == Vs{"23"});

    // Identifiers MUST comprise only ASCII alphanumerics and hyphen.
    // Identifiers MUST NOT be empty.
    REQUIRE_INVALID_VERSION("1.2.3-a,");
    REQUIRE_INVALID_VERSION("1.2.3-..");

    // Numeric identifiers MUST NOT include leading zeroes.
    REQUIRE_INVALID_VERSION("1.2.3-a0.01");
    REQUIRE_INVALID_VERSION("1.2.3-00");

    // Mixed alpha-numeric with leading zero is allowed.
    auto v2 = Version("1.2.3-0a.0.000zz");
    REQUIRE(v2.prerelease().value() == Vs{"0a", "0", "000zz"});
}

TEST_CASE("FormatTests: build", "[spec][format]") {
    auto v = Version("1.2.3");
    REQUIRE(v.build().value() == Vs{});

    REQUIRE_INVALID_VERSION("1.2.3 +4");

    // Identifiers MUST comprise only ASCII alphanumerics and hyphen.
    // Identifiers MUST NOT be empty.
    REQUIRE_INVALID_VERSION("1.2.3+a,");
    REQUIRE_INVALID_VERSION("1.2.3+..");

    // Leading zeroes ARE allowed in build identifiers.
    auto v2 = Version("1.2.3+0.0a.01");
    REQUIRE(v2.build().value() == Vs{"0", "0a", "01"});
}

TEST_CASE("FormatTests: precedence", "[spec][format]") {
    // Major, minor, and patch versions are compared numerically.
    // 1.0.0 < 2.0.0 < 2.1.0 < 2.1.1
    REQUIRE(Version("1.0.0") < Version("2.0.0"));
    REQUIRE(Version("2.0.0") < Version("2.1.0"));
    REQUIRE(Version("2.1.0") < Version("2.1.1"));

    // Pre-release has lower precedence than normal.
    REQUIRE(Version("1.0.0-alpha") < Version("1.0.0"));

    // Numeric identifiers compared numerically.
    REQUIRE(Version("1.0.0-1") < Version("1.0.0-2"));

    // Alpha identifiers compared lexically.
    REQUIRE(Version("1.0.0-aa") < Version("1.0.0-ab"));

    // Numeric identifiers always have lower precedence than alpha.
    REQUIRE(Version("1.0.0-9") < Version("1.0.0-a"));

    // Larger set of pre-release fields has higher precedence.
    REQUIRE(Version("1.0.0-a.b.c") < Version("1.0.0-a.b.c.0"));

    // The canonical chain:
    // 1.0.0-alpha < 1.0.0-alpha.1 < 1.0.0-alpha.beta
    // < 1.0.0-beta < 1.0.0-beta.2 < 1.0.0-beta.11 < 1.0.0-rc.1 < 1.0.0
    REQUIRE(Version("1.0.0-alpha") < Version("1.0.0-alpha.1"));
    REQUIRE(Version("1.0.0-alpha.1") < Version("1.0.0-alpha.beta"));
    REQUIRE(Version("1.0.0-alpha.beta") < Version("1.0.0-beta"));
    REQUIRE(Version("1.0.0-beta") < Version("1.0.0-beta.2"));
    REQUIRE(Version("1.0.0-beta.2") < Version("1.0.0-beta.11"));
    REQUIRE(Version("1.0.0-beta.11") < Version("1.0.0-rc.1"));
    REQUIRE(Version("1.0.0-rc.1") < Version("1.0.0"));
}

// ==========================================================================
// test_base.py — TopLevelTestCase
// ==========================================================================

TEST_CASE("TopLevel: compare", "[base][compare]") {
    REQUIRE(semver::compare("0.1.0", "0.1.1") == std::weak_ordering::less);
    REQUIRE(semver::compare("0.1.1", "0.1.1") == std::weak_ordering::equivalent);
    REQUIRE(semver::compare("0.1.1", "0.1.0") == std::weak_ordering::greater);
    REQUIRE(semver::compare("0.1.0-alpha", "0.1.0") == std::weak_ordering::less);

    // Build-only difference throws (NotImplemented equivalent).
    REQUIRE_THROWS(semver::compare("0.1.0-alpha+2", "0.1.0-alpha"));
}

TEST_CASE("TopLevel: match", "[base][match]") {
    REQUIRE(semver::match(">=0.1.1", "0.1.2"));
    REQUIRE(semver::match(">=0.1.1", "0.1.1"));
    REQUIRE(semver::match(">=0.1.1", "0.1.2-alpha"));
    REQUIRE(semver::match(">=0.1.1,!=0.2.0", "0.2.1"));
}

TEST_CASE("TopLevel: validate valid", "[base][validate]") {
    const std::vector<std::string> valids = {
        "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-beta.2", "1.0.0-beta.11",
        "1.0.0-rc.1", "1.0.0-rc.1+build.1", "1.0.0", "1.0.0+0.3.7",
        "1.3.7+build", "1.3.7+build.2.b8f12d7", "1.3.7+build.11.e0f985a",
        "1.1.1", "1.1.2", "1.1.3-rc4.5",
        "1.1.3-rc42.3-14-15.24+build.2012-04-13.223",
        "1.1.3+build.2012-04-13.HUY.alpha-12.1",
    };
    for (auto& v : valids) {
        INFO("version: " << v);
        REQUIRE(semver::validate(v));
    }
}

TEST_CASE("TopLevel: validate invalid", "[base][validate]") {
    const std::vector<std::string> invalids = {
        "1", "v1", "1.2.3.4", "1.2", "1.2a3", "1.2.3a4", "v12.34.5", "1.2.3+4+5",
    };
    for (auto& v : invalids) {
        INFO("version: " << v);
        REQUIRE_FALSE(semver::validate(v));
    }
}

// ==========================================================================
// test_base.py — VersionTestCase: parsing
// ==========================================================================

TEST_CASE("Version: parsing known versions", "[base][parsing]") {
    using T = std::tuple<std::string, int, int, int, Vs, Vs>;
    std::vector<T> cases = {
        {"1.0.0-alpha",    1,0,0, {"alpha"},       {}},
        {"1.0.0-alpha.1",  1,0,0, {"alpha","1"},   {}},
        {"1.0.0-beta.2",   1,0,0, {"beta","2"},    {}},
        {"1.0.0-beta.11",  1,0,0, {"beta","11"},   {}},
        {"1.0.0-rc.1",     1,0,0, {"rc","1"},      {}},
        {"1.0.0-rc.1+build.1", 1,0,0, {"rc","1"}, {"build","1"}},
        {"1.0.0",          1,0,0, {},               {}},
        {"1.0.0+0.3.7",    1,0,0, {},               {"0","3","7"}},
        {"1.3.7+build",    1,3,7, {},               {"build"}},
        {"1.3.7+build.2.b8f12d7", 1,3,7, {},        {"build","2","b8f12d7"}},
        {"1.3.7+build.11.e0f985a", 1,3,7, {},       {"build","11","e0f985a"}},
        {"1.1.1",          1,1,1, {},               {}},
        {"1.1.2",          1,1,2, {},               {}},
        {"1.1.3-rc4.5",    1,1,3, {"rc4","5"},     {}},
        {"1.1.3-rc42.3-14-15.24+build.2012-04-13.223",
                            1,1,3, {"rc42","3-14-15","24"}, {"build","2012-04-13","223"}},
        {"1.1.3+build.2012-04-13.HUY.alpha-12.1",
                            1,1,3, {},               {"build","2012-04-13","HUY","alpha-12","1"}},
    };

    for (auto& [text, mj, mn, pa, pre, bld] : cases) {
        INFO("version: " << text);
        auto v = Version(text);
        REQUIRE(v.major() == mj);
        REQUIRE(v.minor() == mn);
        REQUIRE(v.patch() == pa);
        REQUIRE(v.prerelease().value() == pre);
        REQUIRE(v.build().value() == bld);
    }
}

TEST_CASE("Version: str round-trip", "[base][str]") {
    std::vector<std::string> versions = {
        "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-beta.2", "1.0.0-beta.11",
        "1.0.0-rc.1", "1.0.0-rc.1+build.1", "1.0.0", "1.0.0+0.3.7",
        "1.3.7+build", "1.3.7+build.2.b8f12d7", "1.1.1", "1.1.2",
        "1.1.3-rc4.5", "1.1.3-rc42.3-14-15.24+build.2012-04-13.223",
    };
    for (auto& text : versions) {
        INFO("version: " << text);
        REQUIRE(Version(text).to_string() == text);
    }
}

TEST_CASE("Version: equality and compare_to_self", "[base][eq]") {
    std::vector<std::string> versions = {
        "1.0.0-alpha", "1.0.0", "1.3.7+build", "1.1.3-rc4.5",
    };
    for (auto& text : versions) {
        INFO("version: " << text);
        REQUIRE(Version(text) == Version(text));
    }
}

TEST_CASE("Version: hash", "[base][hash]") {
    std::set<std::size_t> hashes;
    hashes.insert(Version("0.1.0").hash());
    hashes.insert(Version("0.1.0").hash());
    // Same version hashes to same value.
    REQUIRE(Version("0.1.0").hash() == Version("0.1.0").hash());
}

// ==========================================================================
// test_base.py — bump_build_versions (next_major/minor/patch with +build)
// ==========================================================================

TEST_CASE("Version: next_major/minor/patch with build metadata", "[base][bump]") {
    SECTION("1.0.0+build") {
        auto v = Version("1.0.0+build");
        auto nm = v.next_major();
        REQUIRE(nm.major() == 2); REQUIRE(nm.minor() == 0); REQUIRE(nm.patch() == 0);
        REQUIRE(nm.prerelease().value().empty()); // no build on next_*

        auto nmi = v.next_minor();
        REQUIRE(nmi.major() == 1); REQUIRE(nmi.minor() == 1); REQUIRE(nmi.patch() == 0);

        auto np = v.next_patch();
        REQUIRE(np.major() == 1); REQUIRE(np.minor() == 0); REQUIRE(np.patch() == 1);
    }

    SECTION("1.1.0+build") {
        auto v = Version("1.1.0+build");
        REQUIRE(v.next_major().to_string() == "2.0.0");
        REQUIRE(v.next_minor().to_string() == "1.2.0");
        REQUIRE(v.next_patch().to_string() == "1.1.1");
    }

    SECTION("1.0.1+build") {
        auto v = Version("1.0.1+build");
        REQUIRE(v.next_major().to_string() == "2.0.0");
        REQUIRE(v.next_minor().to_string() == "1.1.0");
        REQUIRE(v.next_patch().to_string() == "1.0.2");
    }
}

// ==========================================================================
// test_base.py — bump_prerelease_versions
// ==========================================================================

TEST_CASE("Version: next_major/minor/patch with prerelease", "[base][bump_pre]") {
    SECTION("1.0.0-pre+build") {
        auto v = Version("1.0.0-pre+build");
        auto nm = v.next_major();
        REQUIRE(nm.major() == 1); REQUIRE(nm.minor() == 0); REQUIRE(nm.patch() == 0);
        REQUIRE(nm.prerelease().value().empty());

        auto nmi = v.next_minor();
        REQUIRE(nmi.major() == 1); REQUIRE(nmi.minor() == 0); REQUIRE(nmi.patch() == 0);

        auto np = v.next_patch();
        REQUIRE(np.major() == 1); REQUIRE(np.minor() == 0); REQUIRE(np.patch() == 0);
    }

    SECTION("1.1.0-pre+build") {
        auto v = Version("1.1.0-pre+build");
        auto nm = v.next_major();
        REQUIRE(nm.major() == 2); REQUIRE(nm.minor() == 0); REQUIRE(nm.patch() == 0);

        auto nmi = v.next_minor();
        REQUIRE(nmi.major() == 1); REQUIRE(nmi.minor() == 1); REQUIRE(nmi.patch() == 0);

        auto np = v.next_patch();
        REQUIRE(np.major() == 1); REQUIRE(np.minor() == 1); REQUIRE(np.patch() == 0);
    }

    SECTION("1.0.1-pre+build") {
        auto v = Version("1.0.1-pre+build");
        auto nm = v.next_major();
        REQUIRE(nm.major() == 2); REQUIRE(nm.minor() == 0); REQUIRE(nm.patch() == 0);

        auto nmi = v.next_minor();
        REQUIRE(nmi.major() == 1); REQUIRE(nmi.minor() == 1); REQUIRE(nmi.patch() == 0);

        auto np = v.next_patch();
        REQUIRE(np.major() == 1); REQUIRE(np.minor() == 0); REQUIRE(np.patch() == 1);
    }
}

// ==========================================================================
// test_base.py — truncate
// ==========================================================================

TEST_CASE("Version: truncate", "[base][truncate]") {
    auto v = Version("3.2.1-pre+build");

    REQUIRE(v.truncate("build") == v);
    REQUIRE(v.truncate("prerelease") == Version("3.2.1-pre"));
    REQUIRE(v.truncate("patch") == Version("3.2.1"));
    REQUIRE(v.truncate() == Version("3.2.1"));
    REQUIRE(v.truncate("minor") == Version("3.2.0"));
    REQUIRE(v.truncate("major") == Version("3.0.0"));
}

// ==========================================================================
// test_base.py — CoerceTestCase
// ==========================================================================

TEST_CASE("Version: coerce", "[base][coerce]") {
    struct CoerceCase {
        std::string target;
        std::vector<std::string> samples;
    };
    std::vector<CoerceCase> cases = {
        {"0.0.0",              {"0", "0.0", "0.0.0", "0.0.0+", "0-"}},
        {"0.1.0",              {"0.1", "0.1+", "0.1-", "0.1.0", "0.01.0", "000.0001.0000000000"}},
        {"0.1.0+2",            {"0.1.0+2", "0.1.0.2"}},
        {"0.1.0+2.3.4",        {"0.1.0+2.3.4", "0.1.0+2+3+4", "0.1.0.2+3+4"}},
        {"0.1.0+2-3.4",        {"0.1.0+2-3.4", "0.1.0+2-3+4", "0.1.0.2-3+4"}},
        {"0.1.0-a2.3",         {"0.1.0-a2.3", "0.1.0a2.3"}},
        {"0.1.0-a2.3+4.5-6",   {"0.1.0-a2.3+4.5-6", "0.1.0a2.3+4.5-6"}},
    };
    for (auto& [target_str, samples] : cases) {
        auto target = Version(target_str);
        for (auto& sample : samples) {
            INFO("target=" << target_str << " sample=" << sample);
            auto coerced = Version::coerce(sample);
            REQUIRE(coerced == target);
        }
    }
}

TEST_CASE("Version: coerce invalid", "[base][coerce]") {
    REQUIRE_THROWS_AS(Version::coerce("v1"), std::invalid_argument);
}

// ==========================================================================
// test_base.py — SpecItem matches (SimpleSpec single-clause matching)
// ==========================================================================

TEST_CASE("SimpleSpec: single-clause matching", "[spec][simple][match]") {
    struct SpecMatchCase {
        std::string spec;
        std::vector<std::string> matching;
        std::vector<std::string> failing;
    };

    std::vector<SpecMatchCase> cases = {
        {"==0.1.0",
            {"0.1.0", "0.1.0+build1"},
            {"0.0.1", "0.1.0-rc1", "0.2.0", "0.1.1", "0.1.0-rc1+build2"}},
        {"==0.1.2-rc3",
            {"0.1.2-rc3+build1", "0.1.2-rc3+build4.5"},
            {"0.1.2-rc4", "0.1.2", "0.1.3"}},
        {"==0.1.2+build3.14",
            {"0.1.2+build3.14"},
            {"0.1.2-rc+build3.14", "0.1.2+build3.15"}},
        {"<=0.1.1",
            {"0.0.0", "0.1.1-alpha1", "0.1.1", "0.1.1+build2"},
            {"0.1.2"}},
        {"<0.1.1",
            {"0.1.0", "0.0.0"},
            {"0.1.1", "0.1.1-zzz+999", "1.2.0", "0.1.1+build3"}},
        {"<=0.1.2",
            {"0.1.2+build4", "0.1.2-alpha", "0.1.0"},
            {"0.2.3", "1.1.1", "0.1.3"}},
        {"<0.1.1-",
            {"0.1.0", "0.1.1-alpha", "0.1.1-alpha+4"},
            {"0.2.0", "1.0.0", "0.1.1", "0.1.1+build1"}},
        {">=0.2.3-rc2",
            {"0.2.3-rc3", "0.2.3", "0.2.3+1", "0.2.3-rc2", "0.2.3-rc2+1"},
            {"0.2.3-rc1", "0.2.2"}},
        {">=0.2.3",
            {"0.2.3", "0.2.3+1"},
            {"0.2.3-rc3", "0.2.3-rc2", "0.2.3-rc2+1", "0.2.3-rc1", "0.2.2"}},
        {"==0.2.3+",
            {"0.2.3"},
            {"0.2.3+rc1", "0.2.4", "0.2.3-rc2"}},
        {"!=0.2.3-rc2+12",
            {"0.2.3-rc3", "0.2.3", "0.2.3-rc2+1", "0.2.4", "0.2.3-rc3+12"},
            {"0.2.3-rc2+12"}},
        {"==2.0.0+b1",
            {"2.0.0+b1"},
            {"2.1.1", "1.9.9", "1.9.9999", "2.0.0", "2.0.0-rc4"}},
        {"!=0.1.1",
            {"0.1.2", "0.1.0", "1.4.2"},
            {"0.1.1", "0.1.1-alpha", "0.1.1+b1"}},
        {"!=0.3.4-",
            {"0.4.0", "1.3.0", "0.3.4-alpha", "0.3.4-alpha+b1"},
            {"0.3.4", "0.3.4+b1"}},
        {"~1.1.2",
            {"1.1.3", "1.1.2+b1"},
            {"1.1.1", "1.1.2-alpha", "1.1.2-alpha+b1", "1.2.1", "2.1.0"}},
        {"^1.1.2",
            {"1.1.3", "1.1.2+b1", "1.2.1"},
            {"1.1.1", "1.1.2-alpha", "1.1.2-alpha+b1", "2.1.0"}},
        {"^0.1.2",
            {"0.1.2", "0.1.2+b1", "0.1.3"},
            {"0.1.2-alpha", "0.2.0", "1.1.2", "0.1.1"}},
        {"^0.0.2",
            {"0.0.2", "0.0.2+abb"},
            {"0.0.2-alpha", "0.1.0", "0.0.3", "1.0.0"}},
        {"~=1.4.5",
            {"1.4.5", "1.4.10-alpha", "1.4.10"},
            {"1.3.6", "1.4.4", "1.4.5-alpha", "1.5.0"}},
        {"~=1.4.0",
            {"1.4.0", "1.4.10-alpha", "1.4.10"},
            {"1.3.6", "1.3.9", "1.4.0-alpha", "1.5.0"}},
        {"~=1.4",
            {"1.4.0", "1.6.10-alpha", "1.6.10"},
            {"1.3.0", "1.4.0-alpha", "2.0.0"}},
    };

    for (auto& [spec_text, matching, failing] : cases) {
        auto spec = SimpleSpec(spec_text);
        for (auto& ver : matching) {
            INFO("spec=" << spec_text << " should match " << ver);
            REQUIRE(spec.match(Version(ver)));
        }
        for (auto& ver : failing) {
            INFO("spec=" << spec_text << " should NOT match " << ver);
            REQUIRE_FALSE(spec.match(Version(ver)));
        }
    }
}

// ==========================================================================
// test_base.py — SpecTestCase matches (comma-separated compound specs)
// ==========================================================================

TEST_CASE("SimpleSpec: compound spec matching", "[spec][simple][compound]") {
    struct CompoundCase {
        std::string spec;
        std::vector<std::string> matching;
        std::vector<std::string> failing;
    };

    std::vector<CompoundCase> cases = {
        {"==0.1.0",
            {"0.1.0", "0.1.0+build1"},
            {"0.0.1", "0.1.0-rc1", "0.2.0", "0.1.1", "0.1.0-rc1+build2"}},
        {"<=0.1.1",
            {"0.0.0", "0.1.1-alpha1", "0.1.1", "0.1.1+build2"},
            {"0.1.2"}},
        {"<0.1.1",
            {"0.1.0", "0.0.0"},
            {"0.1.1", "0.1.1-zzz+999", "1.2.0", "0.1.1+build3"}},
        {"==0.1.*",
            {"0.1.1", "0.1.1+4", "0.1.0", "0.1.99"},
            {"0.1.0-alpha", "0.0.1", "0.2.0"}},
        {"==1.*",
            {"1.1.1", "1.1.0+4", "1.1.0", "1.99.99"},
            {"1.0.0-alpha", "0.1.0", "2.0.0"}},
        {">=0.1.0-,!=0.1.3-rc1,!=0.1.0+b3,<0.1.4",
            {"0.1.1", "0.1.0+b4", "0.1.2", "0.1.3-rc2"},
            {"0.0.1", "0.1.0+b3", "0.1.4", "0.1.4-alpha", "0.1.3-rc1+4",
             "0.1.0-alpha", "0.2.2", "0.1.4-rc1"}},
    };

    for (auto& [spec_text, matching, failing] : cases) {
        auto spec = SimpleSpec(spec_text);
        for (auto& ver : matching) {
            INFO("spec=" << spec_text << " should match " << ver);
            REQUIRE(spec.match(Version(ver)));
        }
        for (auto& ver : failing) {
            INFO("spec=" << spec_text << " should NOT match " << ver);
            REQUIRE_FALSE(spec.match(Version(ver)));
        }
    }
}

// ==========================================================================
// test_base.py — SpecTestCase: filter / select
// ==========================================================================

TEST_CASE("SimpleSpec: filter empty", "[spec][filter]") {
    auto s = SimpleSpec(">=0.1.1");
    auto res = s.filter({});
    REQUIRE(res.empty());
}

TEST_CASE("SimpleSpec: filter incompatible", "[spec][filter]") {
    auto s = SimpleSpec(">=0.1.1,!=0.1.4");
    auto res = s.filter({
        Version("0.1.0"),
        Version("0.1.4"),
        Version("0.1.4-alpha"),
    });
    REQUIRE(res.empty());
}

TEST_CASE("SimpleSpec: filter compatible", "[spec][filter]") {
    auto s = SimpleSpec(">=0.1.1,!=0.1.4,<0.2.0");
    auto res = s.filter({
        Version("0.1.0"),
        Version("0.1.1"),
        Version("0.1.5"),
        Version("0.1.4-alpha"),
        Version("0.1.2"),
        Version("0.2.0"),
    });
    REQUIRE(res.size() == 3);
    // Should contain 0.1.1, 0.1.5, 0.1.2
    auto has = [&](const char* v) {
        return std::find(res.begin(), res.end(), Version(v)) != res.end();
    };
    REQUIRE(has("0.1.1"));
    REQUIRE(has("0.1.5"));
    REQUIRE(has("0.1.2"));
}

TEST_CASE("SimpleSpec: select", "[spec][select]") {
    auto s = SimpleSpec(">=0.1.0");
    auto empty_res = s.select({});
    REQUIRE_FALSE(empty_res.has_value());

    auto res = s.select({Version("0.1.0"), Version("0.1.3"), Version("0.1.1")});
    REQUIRE(res.has_value());
    REQUIRE(*res == Version("0.1.3"));
}

// ==========================================================================
// test_match.py — matching tests (spec accepts version)
// ==========================================================================

TEST_CASE("SimpleSpec: match accepts (test_match.py)", "[spec][match_accept]") {
    struct MatchCase {
        std::string spec;
        std::vector<std::string> versions;
    };

    std::vector<MatchCase> cases = {
        {">=0.1.1", {"0.1.1", "0.1.1+build4.5", "0.1.2-rc1.3", "0.2.0", "1.0.0"}},
        {">0.1.1", {"0.1.2+build4.5", "0.1.2-rc1.3", "0.2.0", "1.0.0"}},
        {"<0.1.1-", {"0.1.1-alpha", "0.1.1-rc4", "0.1.0+12.3"}},
        {"^0.1.2", {"0.1.2", "0.1.2+build4.5", "0.1.3-rc1.3", "0.1.4"}},
        {"~0.1.2", {"0.1.2", "0.1.2+build4.5", "0.1.3-rc1.3"}},
        {"~=1.4.5", {"1.4.5", "1.4.10-alpha", "1.4.10"}},
        {"~=1.4", {"1.4.0", "1.6.10-alpha", "1.6.10"}},
    };

    for (auto& [spec_text, versions] : cases) {
        for (auto& ver : versions) {
            INFO("spec=" << spec_text << " version=" << ver);
            REQUIRE(SimpleSpec(spec_text).match(Version(ver)));
        }
    }
}

// ==========================================================================
// test_npm.py — NpmSpecTests
// ==========================================================================

TEST_CASE("NpmSpec: matching and rejecting", "[npm][match]") {
    struct NpmCase {
        std::string spec;
        std::vector<std::string> matching;
        std::vector<std::string> failing;
    };

    std::vector<NpmCase> cases = {
        {">=1.2.7",
            {"1.2.7", "1.2.8", "1.3.9"},
            {"1.2.6", "1.1.0"}},
        {">=1.2.7 <1.3.0",
            {"1.2.7", "1.2.8", "1.2.99"},
            {"1.2.6", "1.3.0", "1.1.0"}},
        {"1.2.7 || >=1.2.9 <2.0.0",
            {"1.2.7", "1.2.9", "1.4.6"},
            {"1.2.8", "2.0.0"}},
        {">1.2.3-alpha.3",
            {"1.2.3-alpha.7", "3.4.5"},
            {"1.2.3-alpha.3", "3.4.5-alpha.9"}},
        {">=1.2.3-alpha.3",
            {"1.2.3-alpha.3", "1.2.3-alpha.7", "3.4.5"},
            {"1.2.3-alpha.2", "3.4.5-alpha.9"}},
        {">1.2.3-alpha <1.2.3-beta",
            {"1.2.3-alpha.0", "1.2.3-alpha.1"},
            {"1.2.3", "1.2.3-beta.0", "1.2.3-bravo"}},
        {"1.2.3 - 2.3.4",
            {"1.2.3", "1.2.99", "2.2.0", "2.3.4", "2.3.4+b42"},
            {"1.2.0", "1.2.3-alpha.1", "2.3.5"}},
        {"~1.2.3-beta.2",
            {"1.2.3-beta.2", "1.2.3-beta.4", "1.2.4"},
            {"1.2.4-beta.2", "1.3.0"}},
    };

    for (auto& [spec_text, matching, failing] : cases) {
        auto spec = NpmSpec(spec_text);
        for (auto& ver : matching) {
            INFO("npm spec=" << spec_text << " should match " << ver);
            REQUIRE(spec.match(Version(ver)));
        }
        for (auto& ver : failing) {
            INFO("npm spec=" << spec_text << " should NOT match " << ver);
            REQUIRE_FALSE(spec.match(Version(ver)));
        }
    }
}

// ==========================================================================
// test_npm.py — NpmSpec expansion equivalences
// ==========================================================================

TEST_CASE("NpmSpec: expansion equivalences", "[npm][expand]") {
    // For each source expression, the NpmSpec should match/reject
    // the same versions as the expanded form.
    struct Expansion {
        std::string source;
        std::string expanded;
    };

    // We test equivalence by checking a set of probe versions.
    std::vector<std::string> probes = {
        "0.0.0", "0.0.1", "0.1.0", "0.2.3", "0.2.3-beta", "0.2.3-beta.2",
        "1.0.0", "1.0.0-alpha", "1.2.0", "1.2.3", "1.2.3-beta.2", "1.2.4",
        "1.3.0", "1.9.9", "2.0.0", "2.0.0-alpha", "3.0.0",
    };

    std::vector<Expansion> expansions = {
        // Tilde ranges
        {"~1.2.3", ">=1.2.3 <1.3.0"},
        {"~1.2",   ">=1.2.0 <1.3.0"},
        {"~1",     ">=1.0.0 <2.0.0"},
        {"~0.2.3", ">=0.2.3 <0.3.0"},
        {"~0.2",   ">=0.2.0 <0.3.0"},
        {"~0",     ">=0.0.0 <1.0.0"},
        // Caret ranges
        {"^1.2.3", ">=1.2.3 <2.0.0"},
        {"^0.2.3", ">=0.2.3 <0.3.0"},
        {"^0.0.3", ">=0.0.3 <0.0.4"},
    };

    for (auto& [source, expanded] : expansions) {
        auto src_spec = NpmSpec(source);
        auto exp_spec = NpmSpec(expanded);
        for (auto& probe : probes) {
            INFO("source=" << source << " expanded=" << expanded << " probe=" << probe);
            REQUIRE(src_spec.match(Version(probe)) == exp_spec.match(Version(probe)));
        }
    }
}

// ==========================================================================
// Version comparison operators
// ==========================================================================

TEST_CASE("Version: comparison operators", "[base][comparison]") {
    REQUIRE(Version("0.1.0") < Version("0.1.1"));
    REQUIRE(Version("0.1.1") <= Version("0.1.1"));
    REQUIRE(Version("0.1.1") == Version("0.1.1"));
    REQUIRE(Version("0.1.2") > Version("0.1.1"));
    REQUIRE(Version("0.1.2") >= Version("0.1.1"));
    REQUIRE(Version("0.1.0") != Version("0.1.1"));

    // Build metadata is included in equality.
    REQUIRE_FALSE(Version("0.1.2") == Version("0.1.2+git2"));

    // But <=/>= still hold because precedence ignores build.
    REQUIRE(Version("0.1.2") <= Version("0.1.2+git2"));
    REQUIRE(Version("0.1.2") >= Version("0.1.2+git2"));
}

// ==========================================================================
// Version construction from components
// ==========================================================================

TEST_CASE("Version: construction from components", "[base][ctor]") {
    auto v = Version(1, 2, 3);
    REQUIRE(v.to_string() == "1.2.3");

    auto v2 = Version(0, 1, 2, Vs{"alpha", "2"});
    REQUIRE(v2.to_string() == "0.1.2-alpha.2");

    auto v3 = Version(1, 0, 0, Vs{"rc", "1"}, Vs{"build", "1"});
    REQUIRE(v3.to_string() == "1.0.0-rc.1+build.1");
}

// ==========================================================================
// test_base.py — next_major/minor/patch reference examples
// ==========================================================================

TEST_CASE("Version: next_major reference", "[base][next]") {
    REQUIRE(Version("1.0.2").next_major() == Version("2.0.0"));
    REQUIRE(Version("1.0.0-alpha").next_major() == Version("1.0.0"));
}

TEST_CASE("Version: next_minor reference", "[base][next]") {
    REQUIRE(Version("1.0.2").next_minor() == Version("1.1.0"));
    REQUIRE(Version("1.1.2-alpha").next_minor() == Version("1.2.0"));
    REQUIRE(Version("1.1.0-alpha").next_minor() == Version("1.1.0"));
}

TEST_CASE("Version: next_patch reference", "[base][next]") {
    REQUIRE(Version("1.0.2").next_patch() == Version("1.0.3"));
    REQUIRE(Version("1.0.2-alpha").next_patch() == Version("1.0.2"));
}

// ==========================================================================
// Truncate reference examples
// ==========================================================================

TEST_CASE("Version: truncate reference", "[base][truncate_ref]") {
    auto v = Version("1.0.2-rc1+b43.24");
    REQUIRE(v.truncate() == Version("1.0.2"));
    REQUIRE(v.truncate("minor") == Version("1.0.0"));
    REQUIRE(v.truncate("prerelease") == Version("1.0.2-rc1"));
}

// ==========================================================================
// Partial version parsing (basic coverage)
// ==========================================================================

TEST_CASE("Version: partial parsing", "[base][partial]") {
    auto v1 = Version("1", true);
    REQUIRE(v1.major() == 1);
    REQUIRE_FALSE(v1.minor().has_value());
    REQUIRE_FALSE(v1.patch().has_value());

    auto v2 = Version("1.2", true);
    REQUIRE(v2.major() == 1);
    REQUIRE(v2.minor() == 2);
    REQUIRE_FALSE(v2.patch().has_value());

    auto v3 = Version("1.2.3", true);
    REQUIRE(v3.major() == 1);
    REQUIRE(v3.minor() == 2);
    REQUIRE(v3.patch() == 3);

    // Partial with prerelease.
    auto v4 = Version("1.2.3-alpha", true);
    REQUIRE(v4.prerelease().value() == Vs{"alpha"});
}

TEST_CASE("Version: partial str round-trip", "[base][partial_str]") {
    std::vector<std::string> partials = {"1", "1.2", "1.2.3", "1.2.3-alpha"};
    for (auto& text : partials) {
        INFO("partial: " << text);
        REQUIRE(Version(text, true).to_string() == text);
    }
}

// ==========================================================================
// SimpleSpec equality / str
// ==========================================================================

TEST_CASE("SimpleSpec: str round-trip", "[spec][str]") {
    auto s = SimpleSpec(">=0.1.1,!=0.1.2");
    REQUIRE(s.str() == ">=0.1.1,!=0.1.2");
}

// ==========================================================================
// validate() convenience
// ==========================================================================

TEST_CASE("validate: free function", "[validate]") {
    REQUIRE(semver::validate("1.1.1"));
    REQUIRE_FALSE(semver::validate("1.2.3a4"));
}

// ==========================================================================
// match() convenience
// ==========================================================================

TEST_CASE("match: free function", "[match]") {
    REQUIRE(semver::match(">=0.1.1", "0.1.2"));
    REQUIRE_FALSE(semver::match(">0.1.1", "0.1.1"));
}
