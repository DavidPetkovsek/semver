# semver

[![CI](https://github.com/DavidPetkovsek/semver/actions/workflows/ci.yml/badge.svg)](https://github.com/DavidPetkovsek/semver/actions/workflows/ci.yml)

A C++20 semantic versioning library — a faithful translation of [python-semanticversion](https://github.com/rbarrois/python-semanticversion).

Parse, compare, and match versions against flexible range specifications following the [SemVer 2.0.0](https://semver.org/) standard. Includes both a simple/intuitive spec syntax and full [NPM-style range](https://github.com/npm/node-semver#ranges) support.

## Features

- **Full SemVer 2.0.0** — major, minor, patch, prerelease identifiers, and build metadata
- **Version comparison** with correct precedence rules
- **SimpleSpec** — comma-separated clauses with wildcards, caret, tilde, and compatible-release operators
- **NpmSpec** — the complete NPM range specification including hyphen ranges, X-ranges, and `||` unions
- **Partial versions** — parse incomplete version strings like `1` or `1.2`
- **Coercion** — best-effort conversion of arbitrary strings into valid versions
- **Static or shared library** — configurable at build time via CMake

## Building

Build as a shared library:

```bash
cmake -B build -DSEMVER_BUILD_TESTS=OFF
cmake --build build
```

Build as a static library:

```bash
cmake -B build -DSEMVER_BUILD_TESTS=OFF -DSEMVER_BUILD_SHARED=OFF
cmake --build build
```

### CMake

After installing, consume from your own project:

```cmake
find_package(semver REQUIRED)
target_link_libraries(myapp PRIVATE semver::semver)
```

## Usage

```cpp
#include <semver/semver.hpp>
```

All types live in the `semver` namespace.

---

### Parsing versions

Construct a `Version` from a string:

```cpp
auto v = semver::Version("1.4.2-rc.1+build.47");

std::cout << v.major;                  // 1
std::cout << v.minor.value();          // 4
std::cout << v.patch.value();          // 2
std::cout << (*v.prerelease)[0];       // "rc"
std::cout << (*v.prerelease)[1];       // "1"
std::cout << (*v.build)[0];            // "build"
std::cout << (*v.build)[1];            // "47"
std::cout << v.to_string();            // "1.4.2-rc.1+build.47"
```

Validate without throwing:

```cpp
semver::Version::validate("1.2.3");    // true
semver::Version::validate("not.a.v");  // false
```

### Partial versions

```cpp
auto p = semver::Version("1.2", /*partial=*/true);
// p.patch is std::nullopt
```

### Coercion

Best-effort conversion of messy strings:

```cpp
auto v = semver::Version::coerce("02");          // 2.0.0
auto w = semver::Version::coerce("1.2.3.4");     // 1.2.3+4
```

---

### Comparing versions

All the standard comparison operators work and follow SemVer 2.0.0 precedence:

```cpp
auto a = semver::Version("1.0.0");
auto b = semver::Version("1.0.1");
auto c = semver::Version("1.0.0-alpha");
auto d = semver::Version("1.0.0-beta");

a < b;    // true  — patch bump
c < a;    // true  — prereleases sort before the release
c < d;    // true  — "alpha" < "beta" lexicographically

a == semver::Version("1.0.0+anything");  // false — build metadata is compared for equality
a >= c;                                   // true
```

Numeric prerelease identifiers sort numerically, not lexicographically:

```cpp
semver::Version("1.0.0-2")  < semver::Version("1.0.0-10");   // true
semver::Version("1.0.0-rc") > semver::Version("1.0.0-10");   // true — alpha > numeric
```

---

### Bumping versions

```cpp
auto v = semver::Version("1.2.3-rc.1+build.5");

v.next_major().to_string();   // "2.0.0"
v.next_minor().to_string();   // "1.3.0"
v.next_patch().to_string();   // "1.2.3"  — strips the prerelease
```

When a prerelease is present the bump returns the smallest version that is strictly greater. For example `1.2.3-rc.1` already sorts below `1.2.3`, so `next_patch()` returns `1.2.3` (not `1.2.4`).

### Truncating versions

```cpp
auto v = semver::Version("3.2.1-pre+build");

v.truncate("build").to_string();       // "3.2.1-pre+build"  (copy)
v.truncate("prerelease").to_string();  // "3.2.1-pre"
v.truncate("patch").to_string();       // "3.2.1"
v.truncate("minor").to_string();       // "3.2.0"
v.truncate("major").to_string();       // "3.0.0"
```

---

### Range matching with SimpleSpec

`SimpleSpec` supports an intuitive comma-separated syntax with wildcards and extended operators.

#### Basic comparison clauses

```cpp
semver::SimpleSpec(">=1.2.0").match(semver::Version("1.3.0"));   // true
semver::SimpleSpec(">=1.2.0").match(semver::Version("1.1.9"));   // false
semver::SimpleSpec("<2.0.0").match(semver::Version("1.99.0"));   // true
semver::SimpleSpec("!=1.5.0").match(semver::Version("1.5.0"));   // false
```

Combine clauses with commas (logical AND):

```cpp
semver::SimpleSpec(">=1.0.0,<2.0.0").match(semver::Version("1.7.3"));  // true
semver::SimpleSpec(">=1.0.0,<2.0.0").match(semver::Version("2.0.0"));  // false
```

#### Wildcards

```cpp
semver::SimpleSpec("==1.2.*").match(semver::Version("1.2.0"));   // true
semver::SimpleSpec("==1.2.*").match(semver::Version("1.2.99"));  // true
semver::SimpleSpec("==1.2.*").match(semver::Version("1.3.0"));   // false

semver::SimpleSpec("==1.*").match(semver::Version("1.0.0"));     // true
semver::SimpleSpec("==1.*").match(semver::Version("1.99.0"));    // true
```

#### Tilde — patch-level flexibility

`~X.Y.Z` matches `>=X.Y.Z` and `<X.(Y+1).0`:

```cpp
auto spec = semver::SimpleSpec("~1.4.2");
spec.match(semver::Version("1.4.2"));   // true
spec.match(semver::Version("1.4.9"));   // true
spec.match(semver::Version("1.5.0"));   // false
```

#### Caret — compatible with a version

`^X.Y.Z` allows changes that do not modify the left-most non-zero digit:

```cpp
auto spec = semver::SimpleSpec("^1.2.3");
spec.match(semver::Version("1.2.3"));   // true
spec.match(semver::Version("1.9.0"));   // true
spec.match(semver::Version("2.0.0"));   // false

// For 0.x the caret is more restrictive:
semver::SimpleSpec("^0.2.3").match(semver::Version("0.2.9"));   // true
semver::SimpleSpec("^0.2.3").match(semver::Version("0.3.0"));   // false
```

#### Compatible release (PyPI-style)

`~=X.Y` is equivalent to `>=X.Y.0,<(X+1).0.0`:

```cpp
auto spec = semver::SimpleSpec("~=1.4");
spec.match(semver::Version("1.4.0"));   // true
spec.match(semver::Version("1.99.0"));  // true
spec.match(semver::Version("2.0.0"));   // false
```

#### Prerelease handling

By default, a prerelease like `1.0.0-alpha` does **not** satisfy `<1.0.0` because the common expectation is that prereleases belong to their own release. Append a bare hyphen to opt in:

```cpp
semver::SimpleSpec("<1.0.0").match(semver::Version("1.0.0-alpha"));    // false
semver::SimpleSpec("<1.0.0-").match(semver::Version("1.0.0-alpha"));   // true
```

#### Build metadata

Build metadata has no ordering. The only meaningful operation is exact equality:

```cpp
semver::SimpleSpec("==1.0.0+build.42").match(semver::Version("1.0.0+build.42"));  // true
semver::SimpleSpec("==1.0.0+build.42").match(semver::Version("1.0.0+build.99"));  // false
semver::SimpleSpec("<=1.0.0").match(semver::Version("1.0.0+anything"));            // true — build ignored
```

#### Filtering and selecting

```cpp
std::vector<semver::Version> versions = {
    semver::Version("0.9.0"),
    semver::Version("1.0.0"),
    semver::Version("1.3.0"),
    semver::Version("2.0.0"),
};

auto spec = semver::SimpleSpec(">=1.0.0,<2.0.0");

auto filtered = spec.filter(versions);
// filtered: [1.0.0, 1.3.0]

auto best = spec.select(versions);
// best: 1.3.0
```

---

### Range matching with NpmSpec

`NpmSpec` implements the full [node-semver](https://github.com/npm/node-semver#ranges) range specification.

#### Space-separated intersections

Clauses separated by spaces are ANDed together:

```cpp
auto spec = semver::NpmSpec(">=1.2.7 <1.3.0");
spec.match(semver::Version("1.2.7"));   // true
spec.match(semver::Version("1.2.99"));  // true
spec.match(semver::Version("1.3.0"));   // false
```

#### Union with `||`

```cpp
auto spec = semver::NpmSpec("1.2.7 || >=1.2.9 <2.0.0");
spec.match(semver::Version("1.2.7"));   // true
spec.match(semver::Version("1.2.8"));   // false — not in either range
spec.match(semver::Version("1.4.6"));   // true
```

#### X-Ranges

Wildcards (`*`, `x`, `X`) or missing components mean "any value":

```cpp
semver::NpmSpec("*").match(semver::Version("99.99.99"));            // true
semver::NpmSpec("1.x").match(semver::Version("1.0.0"));             // true
semver::NpmSpec("1.x").match(semver::Version("1.99.0"));            // true
semver::NpmSpec("1.x").match(semver::Version("2.0.0"));             // false
semver::NpmSpec("1.2.x").match(semver::Version("1.2.0"));           // true
semver::NpmSpec("1.2.x").match(semver::Version("1.3.0"));           // false
```

#### Hyphen ranges

`A - B` is equivalent to `>=A <=B`, with partial versions expanded:

```cpp
auto spec = semver::NpmSpec("1.2.3 - 2.3.4");
spec.match(semver::Version("1.2.3"));   // true
spec.match(semver::Version("2.3.4"));   // true
spec.match(semver::Version("2.3.5"));   // false

// Partial upper bound: "1.2.3 - 2.3" means ">=1.2.3 <2.4.0"
semver::NpmSpec("1.2.3 - 2.3").match(semver::Version("2.3.99"));   // true
semver::NpmSpec("1.2.3 - 2.3").match(semver::Version("2.4.0"));    // false
```

#### Tilde ranges

`~X.Y.Z` allows patch-level changes. If minor is missing, minor-level changes are allowed:

```cpp
semver::NpmSpec("~1.2.3").match(semver::Version("1.2.5"));   // true
semver::NpmSpec("~1.2.3").match(semver::Version("1.3.0"));   // false

semver::NpmSpec("~1.2").match(semver::Version("1.2.0"));     // true
semver::NpmSpec("~1.2").match(semver::Version("1.3.0"));     // false

semver::NpmSpec("~1").match(semver::Version("1.9.9"));       // true
semver::NpmSpec("~1").match(semver::Version("2.0.0"));       // false
```

#### Caret ranges

`^X.Y.Z` allows changes that do not modify the left-most non-zero digit:

```cpp
semver::NpmSpec("^1.2.3").match(semver::Version("1.9.0"));   // true
semver::NpmSpec("^1.2.3").match(semver::Version("2.0.0"));   // false

semver::NpmSpec("^0.2.3").match(semver::Version("0.2.9"));   // true
semver::NpmSpec("^0.2.3").match(semver::Version("0.3.0"));   // false

semver::NpmSpec("^0.0.3").match(semver::Version("0.0.3"));   // true
semver::NpmSpec("^0.0.3").match(semver::Version("0.0.4"));   // false
```

#### NPM prerelease behaviour

In NPM semantics, prereleases only satisfy a range if the comparator's version has a prerelease on the **same `major.minor.patch`** tuple:

```cpp
auto spec = semver::NpmSpec(">1.2.3-alpha.3");
spec.match(semver::Version("1.2.3-alpha.7"));   // true  — same patch, higher prerelease
spec.match(semver::Version("3.4.5"));            // true  — release is above the range
spec.match(semver::Version("3.4.5-alpha.9"));   // false — different patch, prerelease blocked
```

#### Combining everything

```cpp
auto spec = semver::NpmSpec("^1.2.0 || >=3.0.0-beta <3.0.1");

spec.match(semver::Version("1.5.0"));           // true  — matched by ^1.2.0
spec.match(semver::Version("2.0.0"));           // false — outside both ranges
spec.match(semver::Version("3.0.0-beta.2"));    // true  — matched by the second range
spec.match(semver::Version("3.0.0"));           // true
spec.match(semver::Version("3.0.1"));           // false
```

---

### Convenience free functions

```cpp
semver::compare("1.2.0", "1.3.0");        // -1
semver::compare("2.0.0", "1.0.0");        //  1
semver::compare("1.0.0", "1.0.0");        //  0

semver::match(">=1.0.0,<2.0.0", "1.5.0"); // true  (uses SimpleSpec)
semver::validate("1.2.3");                 // true
semver::validate("nope");                  // false
```

## License

BSD-2-Clause. See [LICENSE](LICENSE) for details.