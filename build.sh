#!/bin/bash
# build.sh — Compile all C++ source files and tests for PayloadRepo2026
# Run from the repo root: bash build.sh
# Optionally pass "clean" to remove all built binaries: bash build.sh clean

set -e

BUILD_DIR="build"
TESTS_OUT_DIR="$BUILD_DIR/tests"
CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -Wall -Wextra -O2"

# ── Main modules ─────────────────────────────────────────────────────────────
MAIN_SOURCES=(
    "modbus.cpp:modbus"
    "transmittermodule.cpp:transmittermodule"
    "recievermodule.cpp:recievermodule"
    "nema_l298n.cpp:nema_l298n"
)

# ── Test files ────────────────────────────────────────────────────────────────
TEST_SOURCES=(
    "tests/test_modbus.cpp:tests/test_modbus"
    "tests/test_receiver.cpp:tests/test_receiver"
    "tests/test_basestation.cpp:tests/test_basestation"
    "tests/test_stepper.cpp:tests/test_stepper"
    "tests/test_transmitter.cpp:tests/test_transmitter"
    "tests/test_payload_separator.cpp:tests/test_payload_separator"
    "tests/debug_crc.cpp:tests/debug_crc"
)

# ── Clean ─────────────────────────────────────────────────────────────────────
if [[ "${1}" == "clean" ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    exit 0
fi

# ── Prepare output dirs ───────────────────────────────────────────────────────
mkdir -p "$BUILD_DIR" "$TESTS_OUT_DIR"

echo "Compiler : $CXX"
echo "Flags    : $CXXFLAGS"
echo ""

FAILED=()

# ── Build main modules ────────────────────────────────────────────────────────
echo "=== Building main modules ==="
for entry in "${MAIN_SOURCES[@]}"; do
    src="${entry%%:*}"
    out="$BUILD_DIR/${entry##*:}"
    printf "  %-30s -> %s\n" "$src" "$out"
    if $CXX $CXXFLAGS "$src" -o "$out" 2>&1; then
        :
    else
        FAILED+=("$src")
    fi
done

echo ""
echo "=== Building tests ==="
for entry in "${TEST_SOURCES[@]}"; do
    src="${entry%%:*}"
    out="$BUILD_DIR/${entry##*:}"
    printf "  %-40s -> %s\n" "$src" "$out"
    if $CXX $CXXFLAGS "$src" -o "$out" 2>&1; then
        :
    else
        FAILED+=("$src")
    fi
done

echo ""
if [[ ${#FAILED[@]} -gt 0 ]]; then
    echo "BUILD FAILED for:"
    for f in "${FAILED[@]}"; do
        echo "  $f"
    done
    exit 1
else
    echo "All targets built successfully."
    echo ""
    echo "Binaries written to:"
    echo "  $BUILD_DIR/             (main modules)"
    echo "  $BUILD_DIR/tests/       (test binaries)"
fi
