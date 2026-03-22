# tests/fuzz/CMakeLists.txt — libFuzzer harnesses for semver
#
# libFuzzer is built into Clang. If your default compiler supports
# -fsanitize=fuzzer, these targets will build automatically.
#
# Usage:
#   cmake -B build -DSEMVER_BUILD_FUZZ=ON -DSEMVER_BUILD_SHARED=OFF
#   cmake --build build
#
# If your default compiler is not Clang, configure with:
#   cmake -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
#       -DSEMVER_BUILD_FUZZ=ON -DSEMVER_BUILD_SHARED=OFF


include(CheckCXXSourceCompiles)

# --------------------------------------------------------------------------
# Guard: verify the compiler actually supports the flags we need
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
# Flags
# --------------------------------------------------------------------------
set(FUZZ_COMPILE_FLAGS -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g)
set(FUZZ_LINK_FLAGS    -fsanitize=fuzzer,address,undefined)

# --------------------------------------------------------------------------
# Helper: define one fuzz target
# --------------------------------------------------------------------------
function(add_fuzz_target name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE semver::semver)
    target_compile_options(${name} PRIVATE ${FUZZ_COMPILE_FLAGS})
    target_link_options(${name} PRIVATE ${FUZZ_LINK_FLAGS})
endfunction()

# --------------------------------------------------------------------------
# Targets
# --------------------------------------------------------------------------
add_fuzz_target(fuzz_version        fuzz_version.cpp)
add_fuzz_target(fuzz_simple_spec    fuzz_simple_spec.cpp)
add_fuzz_target(fuzz_npm_spec       fuzz_npm_spec.cpp)
add_fuzz_target(fuzz_attempt_parse  fuzz_attempt_parse.cpp)
add_fuzz_target(fuzz_coerce         fuzz_coerce.cpp)