#!/usr/bin/env bash
# tests/fuzz/run.sh — Run all fuzz targets with a shared seed corpus.
#
# Usage:
#   tests/fuzz/run.sh [options] [libfuzzer-args...]
#
# Options:
#   -p, --parallel    Run all targets concurrently
#
# Examples:
#   tests/fuzz/run.sh -max_total_time=60               # sequential, 60s each
#   tests/fuzz/run.sh -p -max_total_time=60             # parallel, 60s each
#   tests/fuzz/run.sh --parallel -max_total_time=0 -runs=10000

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${FUZZ_BUILD_DIR:-$REPO_ROOT/build-fuzz}"
FUZZ_DIR="$BUILD_DIR/tests/fuzz"

TARGETS=(fuzz_version fuzz_simple_spec fuzz_npm_spec fuzz_attempt_parse fuzz_coerce)

PARALLEL=0
FUZZER_ARGS=()

for arg in "$@"; do
    case "$arg" in
        -p|--parallel) PARALLEL=1 ;;
        *)             FUZZER_ARGS+=("$arg") ;;
    esac
done

# Check that at least one target exists
if [[ ! -x "$FUZZ_DIR/${TARGETS[0]}" ]]; then
    echo "Error: $FUZZ_DIR/${TARGETS[0]} not found." >&2
    echo "Build with:" >&2
    echo "  cmake -B build-fuzz -DSEMVER_BUILD_FUZZ=ON -DSEMVER_BUILD_SHARED=OFF" >&2
    echo "  cmake --build build-fuzz" >&2
    exit 1
fi

# Generate seed corpus if it doesn't exist yet
SEED_DIR="$SCRIPT_DIR/corpus"
if [[ ! -d "$SEED_DIR" ]]; then
    echo "Seed corpus not found, generating..."
    python3 "$SCRIPT_DIR/gen_seed_corpus.py"
fi

run_target() {
    local TARGET="$1"
    local WORK_DIR="$SCRIPT_DIR/live_${TARGET}"
    local LOG_FILE="$WORK_DIR/fuzz.log"
    if [ -d "$WORK_DIR" ]; then
        rm -r "$WORK_DIR"
    fi
    mkdir -p "$WORK_DIR"

    if (( PARALLEL )); then
        "$FUZZ_DIR/$TARGET" "$WORK_DIR" "$SEED_DIR" "${FUZZER_ARGS[@]}" \
            > "$LOG_FILE" 2>&1
    else
        echo "═══════════════════════════════════════════════════════"
        echo " Running: $TARGET"
        echo "═══════════════════════════════════════════════════════"
        "$FUZZ_DIR/$TARGET" "$WORK_DIR" "$SEED_DIR" "${FUZZER_ARGS[@]}"
    fi
}

if (( PARALLEL )); then
    echo "Running ${#TARGETS[@]} targets in parallel..."
    echo "Logs: tests/fuzz/live_<target>/fuzz.log"
    echo ""
    echo "Watch progress with:"
    echo "  tail -f tests/fuzz/live_fuzz_*/fuzz.log"
    echo ""

    PIDS=()
    for TARGET in "${TARGETS[@]}"; do
        run_target "$TARGET" &
        PIDS+=("$!:$TARGET")
    done

    FAILED=()
    for entry in "${PIDS[@]}"; do
        PID="${entry%%:*}"
        TARGET="${entry##*:}"
        if wait "$PID"; then
            echo "✓ $TARGET finished"
        else
            echo "✗ $TARGET found a crash (see tests/fuzz/live_${TARGET}/)"
            FAILED+=("$TARGET")
        fi
    done
else
    FAILED=()
    for TARGET in "${TARGETS[@]}"; do
        if run_target "$TARGET"; then
            echo "✓ $TARGET finished"
        else
            echo "✗ $TARGET found a crash (exit $?)"
            FAILED+=("$TARGET")
        fi
        echo ""
    done
fi

if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo ""
    echo "═══════════════════════════════════════════════════════"
    echo " Crashes found in: ${FAILED[*]}"
    echo " Reproducer files are in tests/fuzz/live_<target>/"
    echo "═══════════════════════════════════════════════════════"
    exit 1
else
    echo ""
    echo "All targets passed."
fi