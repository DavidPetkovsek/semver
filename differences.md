# Differences from python-semanticversion

This library is a C++20 translation of [rbarrois/python-semanticversion](https://github.com/rbarrois/python-semanticversion), based on commit [`2cbbee3`](https://github.com/rbarrois/python-semanticversion/commit/2cbbee3) (master, February 28, 2023 — version 2.10.1.dev0).

The translation is faithful to the upstream logic but is not a line-for-line port. This document describes all intentional deviations.

---

## Removed: deprecated features

The upstream library carries several features that were deprecated in 2.7 and slated for removal in 3.0. Since this is a new library with no backwards-compatibility obligations, they have been removed entirely.

### Partial versions

The `partial=True` parameter on `Version` and the ability to construct incomplete versions like `Version("1.2", partial=True)` have been removed. A `Version` always requires all three components (major, minor, patch). Use `SimpleSpec("1.2.*")` or `NpmSpec("1.2.x")` for range matching with partial inputs.

### `Spec` / `LegacySpec`

The `Spec` and `LegacySpec` classes (which accepted multiple comma-joined arguments and exposed a `specs` attribute of `SpecItem` objects) have been removed. Use `SimpleSpec` directly.

### `SpecItem`

The `SpecItem` class has been removed. Clause-level matching is handled internally by the `Clause` hierarchy (`Range`, `AllOf`, `AnyOf`, `Always`, `Never`).

### Django fields

The `semantic_version.django_fields` module (`VersionField`, `SpecField`) has been removed. This is a general-purpose C++ library and has no framework-specific integrations.

### `BaseSpec.parse()` with syntax parameter

The upstream `BaseSpec.parse(expression, syntax='simple')` factory method has been removed. Construct `SimpleSpec` or `NpmSpec` directly.

---

## Bug fixes

### NpmSpec: v-prefix after operators

The upstream Python regex for NpmSpec blocks strips a leading `v` **before** the operator:

```python
^(?:v)?                     # Strip optional initial v
(?P<op><|<=|>=|>|=|\^|~|)   # Operator
```

This means `v1.2.3` works (stripped to `=1.2.3`), but `>=v1.0.0` does **not** match the regex because `v` falls between the operator and the version number — even though the [npm spec](https://docs.npmjs.com/cli/v6/using-npm/semver#versions) states that a leading `v` is stripped from versions.

This library fixes this by stripping the `v` **after** the operator is consumed, so expressions like `>=v1.0.0`, `~v1.2.3`, and `^v0.2.0` all work as expected.

### NpmSpec: empty string input

The upstream Python `"".split("||")` returns `[""]` (a list with one empty string), which then triggers the `if not group: group = '>=0.0.0'` fallback, making `NpmSpec("")` equivalent to `NpmSpec("*")`.

The C++ `split_joiner("")` originally returned an empty vector, causing `NpmSpec("")` to reject all versions. This has been fixed so that `split_joiner` always returns at least one element, matching Python's `str.split()` semantics for a fixed delimiter.

---

## API differences

### Build metadata in equality

Both libraries include build metadata in equality checks (`Version("1.0.0") != Version("1.0.0+build")`), and both ignore build metadata in ordering (`<=`, `>=`). This matches the upstream behavior where `==` checks all fields but `<=>` ignores build.

### `attempt_parse`

A new free function `attempt_parse(string_view, Version&)` is provided as a non-throwing alternative to the `Version` constructor. The upstream library has no direct equivalent (users catch `ValueError`).

### Exceptions vs. error codes

All parsing errors throw `std::invalid_argument`. There is no `noexcept` parsing mode other than `attempt_parse` and `Version::validate`.

### NPM-inspired additions

The upstream Python library does not provide equivalents for these. They are inspired by [npm/node-semver](https://github.com/npm/node-semver), based on commit [`5993c2e`](https://github.com/npm/node-semver/commit/5993c2e) (main, February 5, 2025 — version 7.7.4). `min_version` is defined directly on `BaseSpec`, so both `SimpleSpec` and `NpmSpec` inherit it. `subset` is exposed as a type-safe public method on each derived class individually (see below for rationale).

#### `npm_match`

A free function `npm_match(spec, version)` is provided as a convenience shorthand for `NpmSpec(spec).match(Version(version))`. This mirrors the existing `match()` free function (which uses `SimpleSpec`) and corresponds to node-semver's `satisfies(version, range)`.

```cpp
semver::npm_match("^1.0.0", "1.5.0");  // true
```

#### `NpmSpec::min_version` / `SimpleSpec::min_version`

A member function `min_version()` on `BaseSpec` returns the lowest `Version` that can satisfy the spec, or `std::nullopt` for impossible ranges. This corresponds to node-semver's `minVersion(range)`.

The upstream Python library has no equivalent. Users would have to generate candidate versions manually or use `filter`/`select` over a known set.

```cpp
NpmSpec("^1.2.3").min_version();     // Version("1.2.3")
NpmSpec(">1.0.0").min_version();     // Version("1.0.1")
NpmSpec(">4 <3").min_version();      // std::nullopt
SimpleSpec(">=1.0.0,<2.0.0").min_version(); // Version("1.0.0")
```

#### `NpmSpec::subset` / `SimpleSpec::subset`

A member function `subset(other)` returns `true` if every version matched by `other` is also matched by `*this`. This corresponds to node-semver's `subset(subRange, superRange)`.

The upstream Python library has no equivalent.

Cross-spec comparison (e.g. `SimpleSpec::subset(NpmSpec)`) is intentionally disallowed at the type level. `SimpleSpec` and `NpmSpec` produce clause trees with different prerelease policies (`NATURAL` vs `SAME_PATCH`), which means the same range expression can match different sets of versions depending on which parser was used. For example, `>1.2.3-alpha.3` parsed as a `SimpleSpec` will match `3.4.5-alpha.9` (different patch tuple, prerelease allowed under `NATURAL`), while the same expression parsed as an `NpmSpec` will reject it (prerelease blocked under `SAME_PATCH` because the tuple differs). Comparing clause trees built under different policies would produce silently incorrect results, so each class accepts only its own type.

The shared implementation lives as `BaseSpec::subset_impl(const BaseSpec&)` in the protected section, so no logic is duplicated.

```cpp
NpmSpec(">=1.0.0").subset(NpmSpec("^1.2.3"));           // true  — ^1.2.3 ⊂ >=1.0.0
NpmSpec("^1.2.3").subset(NpmSpec(">=1.0.0"));           // false — >=1.0.0 ⊄ ^1.2.3
SimpleSpec(">=1.0.0").subset(SimpleSpec("^1.2.3"));     // true
// SimpleSpec(">=1.0.0").subset(NpmSpec("^1.2.3"));     // compile error
// NpmSpec("^1.2.3").subset(SimpleSpec(">=1.0.0"));     // compile error
```