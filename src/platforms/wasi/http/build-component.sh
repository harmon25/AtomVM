#!/bin/bash
# Build a WASI component from AtomVM bindings
# This script demonstrates the component model build process

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WASI_SDK_PATH="${WASI_SDK_PATH:-/home/harmon/Libs/wasi-sdk-30}"
ATOMVM_DIR="${SCRIPT_DIR}/../.."
BUILD_DIR="${SCRIPT_DIR}/build"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check for required tools
check_tools() {
    command -v wasm-tools >/dev/null 2>&1 || error "wasm-tools not found. Run: cargo install wasm-tools"
    command -v wit-bindgen >/dev/null 2>&1 || error "wit-bindgen not found. Run: cargo install wit-bindgen-cli"
    [ -f "${WASI_SDK_PATH}/bin/clang" ] || error "wasi-sdk not found at ${WASI_SDK_PATH}"

    info "All required tools found"
}

# Generate bindings
generate_bindings() {
    info "Generating WIT bindings..."

    cd "${SCRIPT_DIR}/wit"
    wit-bindgen c --out-dir .. app.wit || error "Failed to generate bindings"

    info "Bindings generated"
}

# Compile implementation
compile_impl() {
    info "Compiling component implementation..."

    mkdir -p "${BUILD_DIR}"

    "${WASI_SDK_PATH}/bin/clang" \
        -target wasm32-wasip2 \
        -Oz \
        -Wall -Wextra \
        -fvisibility=hidden \
        -I"${SCRIPT_DIR}" \
        --sysroot="${WASI_SDK_PATH}/share/wasi-sysroot" \
        "${SCRIPT_DIR}/host.c" \
        "${SCRIPT_DIR}/host_impl.c" \
        "${SCRIPT_DIR}/host_component_type.o" \
        -o "${BUILD_DIR}/host_core.wasm" \
        -nostartfiles \
        -Wl,--export=__wasi_start=main \
        -Wl,--export=dhandle \
        -Wl,--export=cabi_realloc \
        -Wl,--no-entry \
        -Wl,--strip-all \
        || error "Failed to compile core module"

    info "Core module compiled: ${BUILD_DIR}/host_core.wasm"
}

# Create component
create_component() {
    info "Creating component from core module..."

    # Note: For actual HTTP support, we'd use wasi-http adapter
    # For now, we'll create a minimal component
    wasm-tools component new "${BUILD_DIR}/host_core.wasm" \
        -o "${BUILD_DIR}/host_component.wasm" \
        || error "Failed to create component"

    info "Component created: ${BUILD_DIR}/host_component.wasm"
}

# Validate component
validate_component() {
    info "Validating component..."

    wasm-tools validate "${BUILD_DIR}/host_component.wasm" || error "Component validation failed"

    wasm-tools print "${BUILD_DIR}/host_component.wasm" | head -20

    info "Component is valid"
}

# Print summary
print_summary() {
    cat << EOF

${GREEN}=== Build Complete ===${NC}

Core module: ${BUILD_DIR}/host_core.wasm
Component:    ${BUILD_DIR}/host_component.wasm

To test with wasmtime:
  wasmtime serve -S http ${BUILD_DIR}/host_component.wasm

To test with Spin (if HTTP-compatible):
  spin up

EOF
}

# Main
main() {
    check_tools
    generate_bindings
    compile_impl
    create_component
    validate_component
    print_summary
}

main "$@"