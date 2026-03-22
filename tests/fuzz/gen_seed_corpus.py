"""Generate seed corpus directories for each fuzz target.

Run from the repo root:
    python3 tests/fuzz/gen_seed_corpus.py

Creates:
    tests/fuzz/corpus_version/
    tests/fuzz/corpus_simple_spec/
    tests/fuzz/corpus_npm_spec/
    tests/fuzz/corpus_coerce/

Each directory contains one file per seed input. libFuzzer accepts these
directories as positional arguments:
    ./build/tests/fuzz/fuzz_version tests/fuzz/corpus_version/

Use tests/fuzz/run.sh to run the fuzzers
"""

from pathlib import Path
import hashlib

FUZZ_DIR = Path(__file__).parent

# ── Version seeds ──────────────────────────────────────────────────────────
# Valid versions (from test_semver.cpp and python-semanticversion test_base.py)
VERSION_SEEDS = [
    # basics
    "0.0.0", "0.0.1", "0.1.0", "1.0.0", "1.1.1", "1.1.2", "1.2.3",
    "99.99.99", "100.200.300",
    # prerelease
    "1.0.0-alpha", "1.0.0-alpha.1", "1.0.0-beta.2", "1.0.0-beta.11",
    "1.0.0-rc.1", "1.0.0-0", "1.0.0-0.3.7",
    "1.1.3-rc4.5", "1.1.3-rc42.3-14-15.24",
    "1.2.3-0a.0.000zz", "1.2.3-23",
    # build
    "1.0.0+0.3.7", "1.3.7+build", "1.3.7+build.2.b8f12d7",
    "1.3.7+build.11.e0f985a",
    "1.1.3+build.2012-04-13.HUY.alpha-12.1",
    # prerelease + build
    "1.0.0-rc.1+build.1", "1.0.0-pre+build",
    "1.1.3-rc42.3-14-15.24+build.2012-04-13.223",
    "1.0.0-alpha+001", "1.0.0+20130313144700",
    # invalid (exercise rejection paths)
    "", "1", "1.2", "v1", "v1.2.3", "1.2.3.4",
    "1.2.A", "1.-2.3", "01.2.1", "1.02.1", "1.2.01",
    "1.2.3-a,", "1.2.3-..", "1.2.3-a0.01", "1.2.3-00",
    "1.2.3 -23", "1.2.3 +4", "1.2.3+a,", "1.2.3+..",
    "1.2.3a4", "v12.34.5", "1.2.3+4+5",
    # edge cases
    "\x00", "\t1.0.0", "1.0.0\n", " 1.0.0 ", "1.0.0-",
    "1.0.0+", "1.0.0-+build", "0.0.0-0",
]

# ── SimpleSpec seeds ───────────────────────────────────────────────────────
SIMPLE_SPEC_SEEDS = [
    "*", ">=0.1.1", ">=0.1.1,!=0.2.0", ">=0.1.1,<0.2.0",
    "==1.0.0", "!=1.0.0", "<1.0.0", "<=1.0.0", ">1.0.0", ">=1.0.0",
    "==1.0.0+build", "!=1.0.0-", "==1.0.0+",
    "~=1.4.2", "^1.0.0", "==1.*", "==1.*.*",
    ">=0.1.0,<0.2.0", ">=1.0.0,<2.0.0",
    "<0.1.0-", "<0.1.0-alpha", ">=0.1.1-",
    # invalid
    "", "not_a_spec", ">>1.0.0", ">=", "1.2.3.4",
]

# ── NpmSpec seeds ──────────────────────────────────────────────────────────
NPM_SPEC_SEEDS = [
    # primitives
    ">=1.0.0", ">1.0.0", "<=1.0.0", "<1.0.0", "=1.0.0", "1.0.0",
    # ranges
    ">=1.0.0 <2.0.0", ">=1.2.7 <1.3.0",
    "1.2.7 || >=1.2.9 <2.0.0",
    # hyphen
    "1.0.0 - 2.0.0", "0.0.0 - 1.0.0",
    # x-ranges
    "*", "1.x", "1.2.x", "1.X", "1.2.X", "1.*", "1.2.*", "x", "X",
    "1", "1.2",
    # tilde
    "~1.2.3", "~1.2", "~1", "~0.2.3", "~0.2", "~0",
    "~1.2.3-beta.2",
    # caret
    "^1.2.3", "^0.2.3", "^0.0.3", "^1.2.3-beta.2", "^0.0.3-beta",
    "^1.2.x", "^0.0.x", "^0.0", "^1.x", "^0.x",
    # prerelease
    ">1.2.3-alpha.3", ">=1.0.0-alpha <1.0.0",
    # complex
    ">=1.0.0 <2.0.0 || >=3.0.0",
    "^1.0.0 || ^2.0.0 || ^3.0.0",
    ">=1.0.0  <2.0.0",
    # v-prefix (bug fix area)
    ">=v1.0.0", "~v1.2.3", "^v0.2.0",
    # empty (edge case)
    "",
    # invalid
    "not valid at all", ">>>1.0.0",
]

# ── Coerce seeds ───────────────────────────────────────────────────────────
COERCE_SEEDS = [
    "1", "1.2", "1.2.3", "v1.2.3", "v1", "v1.2",
    "1.2.3.4", "1.2.3.4.5.6",
    "1.2a3", "1.2.3a4", "v12.34.5",
    "release-1.2.3", "ver1.0", "version 2.0.0",
    "", "not a version", "abc",
    "0", "0.0", "0.0.0",
    "999.999.999",
    "1.0.0-alpha+build", "1.0.0+build",
]


def main() -> None:
    out_dir = FUZZ_DIR / "corpus"
    out_dir.mkdir(exist_ok=True)

    all_seeds = set(VERSION_SEEDS + SIMPLE_SPEC_SEEDS + NPM_SPEC_SEEDS + COERCE_SEEDS)

    for s in all_seeds:
        h = hashlib.sha256(s.encode()).hexdigest()[:16]
        (out_dir / h).write_text(s)

    print(f"Generated {len(all_seeds)} seeds in {out_dir}/")


if __name__ == "__main__":
    main()