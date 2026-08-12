#!/bin/bash
# Build the AtomVM HTTP component for Fermyon Spin.
#
# This script:
# 1. Fetches WIT dependencies (from vendored deps.bak/)
# 2. Generates C bindings via wit-bindgen
# 3. Builds AtomVM_http.wasm via CMake
# 4. Validates the resulting WASI component
#
# Prerequisites:
#   - wasi-sdk >= 30 (set WASI_SDK_PATH)
#   - wasm-tools (cargo install wasm-tools)
#   - wit-bindgen-cli (cargo install wit-bindgen-cli)
#   - cmake >= 3.13
#
# Usage:
#   ./build-component.sh           # full build (fetch deps + generate + compile)
#   ./build-component.sh --quick   # skip dep fetch + bindgen if already present

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WASI_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_ROOT="$(cd "${WASI_DIR}/../../.." && pwd)"
WASI_SDK_PATH="${WASI_SDK_PATH:-/home/harmon/Libs/wasi-sdk-30}"
BUILD_DIR="${PROJECT_ROOT}/build-wasi"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# -- Step 0: Check tools ------------------------------------------------------

check_tools() {
    [ -f "${WASI_SDK_PATH}/bin/clang" ] || error "wasi-sdk not found at ${WASI_SDK_PATH}. Set WASI_SDK_PATH."
    command -v cmake      >/dev/null 2>&1 || error "cmake not found"
    command -v wasm-tools >/dev/null 2>&1 || error "wasm-tools not found. Run: cargo install wasm-tools"
    command -v wit-bindgen >/dev/null 2>&1 || error "wit-bindgen not found. Run: cargo install wit-bindgen-cli"

    info "Tools OK"
    info "  wasi-sdk:    ${WASI_SDK_PATH}"
    info "  wit-bindgen:  $(wit-bindgen --version)"
    info "  wasm-tools:   $(wasm-tools --version)"
}

# -- Step 1: Fetch WIT dependencies -------------------------------------------

fetch_wit_deps() {
    if [ -d "${SCRIPT_DIR}/wit/deps" ]; then
        info "wit/deps/ exists — skipping fetch (use --clean to force)"
        return
    fi

    info "Fetching WIT dependencies..."
    "${SCRIPT_DIR}/fetch-wit-deps.sh"
}

# -- Step 2: Generate C bindings ----------------------------------------------

generate_bindings() {
    if [ -f "${SCRIPT_DIR}/generated/app.h" ] && [ -f "${SCRIPT_DIR}/generated/app.c" ]; then
        info "generated/ exists — skipping bindgen (use --clean to force)"
        return
    fi

    if [ ! -d "${SCRIPT_DIR}/wit/deps" ]; then
        error "wit/deps/ missing — run fetch step first"
    fi

    info "Generating C bindings from WIT..."
    mkdir -p "${SCRIPT_DIR}/generated"
    wit-bindgen c "${SCRIPT_DIR}/wit" --out-dir "${SCRIPT_DIR}/generated"
    info "Bindings generated: $(ls "${SCRIPT_DIR}/generated/")"
}

# -- Step 3: Build via CMake ---------------------------------------------------

build_cmake() {
    info "Configuring CMake build..."

    cmake -S "${WASI_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_TOOLCHAIN_FILE="${WASI_DIR}/cmake/wasi-sdk.cmake" \
        -DAVM_BUILD_HTTP_COMPONENT=ON \
        || error "CMake configure failed"

    info "Building AtomVM_http.wasm..."
    cmake --build "${BUILD_DIR}" --target AtomVM_http -j$(nproc 2>/dev/null || echo 4) \
        || error "Build failed"

    info "Built: ${BUILD_DIR}/AtomVM_http.wasm"
}

# -- Step 4: Validate the component -------------------------------------------
# Note: wasm32-wasip2 already produces a valid WASI component.
# No separate `wasm-tools component new` step is needed.

validate_component() {
    local COMPONENT="${BUILD_DIR}/AtomVM_http.wasm"

    info "Validating component..."
    wasm-tools validate --features component-model "${COMPONENT}" \
        || error "Component validation failed"

    info "Component exports:"
    wasm-tools component wit "${COMPONENT}" 2>/dev/null | head -5 || true
    echo "  ..."
    wasm-tools component wit "${COMPONENT}" 2>/dev/null | grep "export " || true

    info "Component is valid"
}

# -- Print summary -------------------------------------------------------------

print_summary() {
    local COMP_SIZE=$(wc -c < "${BUILD_DIR}/AtomVM_http.wasm" 2>/dev/null || echo "?")

    cat << EOF

${GREEN}=== Build Complete ===${NC}

Component:    ${BUILD_DIR}/AtomVM_http.wasm (${COMP_SIZE} bytes)

To test with Spin:
  spin up    # (with spin.toml in current directory)
  curl http://localhost:3000/

EOF
}

# -- Clean generated artifacts -------------------------------------------------

clean() {
    info "Cleaning generated artifacts..."
    rm -rf "${SCRIPT_DIR}/generated"
    rm -rf "${SCRIPT_DIR}/wit/deps"
    info "Cleaned. Run without --clean to rebuild."
}

# -- Main ----------------------------------------------------------------------

main() {
    local do_clean=false
    local quick=false

    for arg in "$@"; do
        case "$arg" in
            --clean) do_clean=true ;;
            --quick) quick=true ;;
            --help|-h)
                echo "Usage: $0 [--quick] [--clean]"
                echo ""
                echo "  --quick   Skip dep fetch and bindgen if artifacts exist"
                echo "  --clean   Remove generated/ and wit/deps/, then exit"
                exit 0
                ;;
        esac
    done

    if $do_clean; then
        clean
        exit 0
    fi

    check_tools

    if ! $quick; then
        # Force regeneration
        rm -rf "${SCRIPT_DIR}/generated"
        rm -rf "${SCRIPT_DIR}/wit/deps"
    fi

    fetch_wit_deps
    generate_bindings
    build_cmake
    validate_component
    print_summary
}

main "$@"
