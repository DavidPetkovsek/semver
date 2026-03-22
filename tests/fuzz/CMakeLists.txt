# tests/fuzz/CMakeLists.txt — libFuzzer harnesses for semver
#
# ASan + UBSan are applied globally by the root CMakeLists.txt when
# SEMVER_BUILD_FUZZ is ON.  This file only adds the libFuzzer engine.
#
# Usage:
#   cmake -B build-fuzz -DSEMVER_BUILD_FUZZ=ON -DSEMVER_BUILD_SHARED=OFF
#   cmake --build build-fuzz
#
# If your default compiler is not Clang, configure with:
#   cmake -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
#       -DSEMVER_BUILD_FUZZ=ON -DSEMVER_BUILD_SHARED=OFF

include(CheckCXXSourceCompiles)

# --------------------------------------------------------------------------
# Guard: verify the compiler supports -fsanitize=fuzzer
# --------------------------------------------------------------------------
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer-no-link")
check_cxx_source_compiles("int main() { return 0; }" SEMVER_HAS_SANITIZE_FUZZER)
unset(CMAKE_REQUIRED_FLAGS)

if(NOT SEMVER_HAS_SANITIZE_FUZZER)
    message(FATAL_ERROR
        "SEMVER_BUILD_FUZZ requires a compiler with -fsanitize=fuzzer support (Clang >= 6).\n"
        "Try: -DCMAKE_CXX_COMPILER=clang++")
endif()

if(SEMVER_BUILD_SHARED)
    message(FATAL_ERROR
        "SEMVER_BUILD_FUZZ requires a static build (libFuzzer provides its own main).\n"
        "Add: -DSEMVER_BUILD_SHARED=OFF")
endif()

# --------------------------------------------------------------------------
# Helper: define one fuzz target
# --------------------------------------------------------------------------
function(add_fuzz_target name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE semver::semver)
    # sanitizer flags are already added via global
    target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
    target_link_options(${name} PRIVATE -fsanitize=fuzzer)
endfunction()

# --------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------
add_fuzz_target(fuzz_version        fuzz_version.cpp)
add_fuzz_target(fuzz_simple_spec    fuzz_simple_spec.cpp)
add_fuzz_target(fuzz_npm_spec       fuzz_npm_spec.cpp)
add_fuzz_target(fuzz_attempt_parse  fuzz_attempt_parse.cpp)
add_fuzz_target(fuzz_coerce         fuzz_coerce.cpp)