#!/bin/bash
# Build and run the Erlang Spin HTTP handler on AtomVM.
#
# This script compiles the spin_handler module, packages it with the platform
# NIF stubs and standard libraries into an app.avm, then launches Spin.
#
# Prerequisites:
#   - erlc on PATH (Erlang/OTP >= 26)
#   - AtomVM built for WASI with HTTP component:
#       export WASI_SDK_PATH=/path/to/wasi-sdk-30
#       cmake -S src/platforms/wasi -B build-wasi \
#           -DCMAKE_TOOLCHAIN_FILE=src/platforms/wasi/cmake/wasi-sdk.cmake \
#           -DAVM_BUILD_HTTP_COMPONENT=ON
#       cmake --build build-wasi --target AtomVM_http
#   - Standard libraries (estdlib.avm, eavmlib.avm) built:
#       cmake --build build -j   # native host build produces libs + PackBEAM
#   - spin CLI on PATH (https://spinframework.dev/)
#
# Usage:
#   ./build_and_run.sh                    # build + run on default port 3000
#   ./build_and_run.sh --port 8080        # build + run on custom port
#   ./build_and_run.sh --build-only       # build without running

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../.." && pwd)"
BUILD_WASI="${PROJECT_ROOT}/build-wasi"
BUILD_HOST="${PROJECT_ROOT}/build"
HTTP_SRC="${PROJECT_ROOT}/src/platforms/wasi/http"
WORK_DIR="${BUILD_WASI}/test"
PORT=3000
BUILD_ONLY=false

# -- Parse arguments -----------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)
            PORT="$2"
            shift 2
            ;;
        --build-only)
            BUILD_ONLY=true
            shift
            ;;
        --help|-h)
            echo "Usage: $0 [--port PORT] [--build-only]"
            echo ""
            echo "  --port PORT     Listen port for Spin (default: 3000)"
            echo "  --build-only    Compile and package only, don't start Spin"
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
done

# -- Colors --------------------------------------------------------------------

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# -- Check prerequisites ------------------------------------------------------

command -v erlc >/dev/null 2>&1 || error "erlc not found. Install Erlang/OTP."

PACKBEAM=""
if [ -x "${BUILD_HOST}/tools/packbeam/PackBEAM" ]; then
    PACKBEAM="${BUILD_HOST}/tools/packbeam/PackBEAM"
elif [ -x "${WORK_DIR}/packbeam" ]; then
    PACKBEAM="${WORK_DIR}/packbeam"
elif command -v packbeam >/dev/null 2>&1; then
    PACKBEAM="$(command -v packbeam)"
else
    error "PackBEAM tool not found. Build AtomVM for host first:\n  cmake --build build -j"
fi

ATOMVM_HTTP="${BUILD_WASI}/AtomVM_http.wasm"
[ -f "${ATOMVM_HTTP}" ] || error "AtomVM_http.wasm not found at ${ATOMVM_HTTP}.\nBuild with: cmake --build build-wasi --target AtomVM_http"

ESTDLIB=""
EAVMLIB=""
for d in "${BUILD_HOST}/libs/estdlib/src" "${BUILD_WASI}/test" "${WORK_DIR}"; do
    [ -f "$d/estdlib.avm" ] && ESTDLIB="$d/estdlib.avm" && break
done
for d in "${BUILD_HOST}/libs/eavmlib/src" "${BUILD_WASI}/test" "${WORK_DIR}"; do
    [ -f "$d/eavmlib.avm" ] && EAVMLIB="$d/eavmlib.avm" && break
done
[ -n "${ESTDLIB}" ] || error "estdlib.avm not found. Build AtomVM for host first."
[ -n "${EAVMLIB}" ] || error "eavmlib.avm not found. Build AtomVM for host first."

info "Tools OK"
info "  PackBEAM:      ${PACKBEAM}"
info "  AtomVM_http:   ${ATOMVM_HTTP}"
info "  estdlib:       ${ESTDLIB}"
info "  eavmlib:       ${EAVMLIB}"

# -- Set up work directory -----------------------------------------------------

mkdir -p "${WORK_DIR}"
BEAMS_DIR="${WORK_DIR}/beams"
mkdir -p "${BEAMS_DIR}"

# -- Step 1: Compile the Erlang handler ----------------------------------------

info "Compiling spin_handler..."
erlc -o "${BEAMS_DIR}" "${SCRIPT_DIR}/spin_handler.erl"

# -- Step 2: Compile platform NIF stubs ----------------------------------------
#
# The spin_http, spin_kv, spin_config, spin_sqlite, and spin_postgres modules
# are Erlang NIF stubs. Their .beam files must be in the AVM pack so the VM can
# load them; the actual function bodies are replaced by C NIFs at import time.

info "Compiling platform NIF stubs..."
erlc -o "${BEAMS_DIR}" \
    "${HTTP_SRC}/spin_http.erl" \
    "${HTTP_SRC}/spin_kv.erl" \
    "${HTTP_SRC}/spin_config.erl" \
    "${HTTP_SRC}/spin_sqlite.erl" \
    "${HTTP_SRC}/spin_postgres.erl"

# -- Step 3: Package into app.avm ---------------------------------------------

info "Packaging app.avm..."
"${PACKBEAM}" "${WORK_DIR}/app.avm" \
    "${BEAMS_DIR}/spin_handler.beam" \
    "${BEAMS_DIR}/spin_http.beam" \
    "${BEAMS_DIR}/spin_kv.beam" \
    "${BEAMS_DIR}/spin_config.beam" \
    "${BEAMS_DIR}/spin_sqlite.beam" \
    "${BEAMS_DIR}/spin_postgres.beam" \
    "${ESTDLIB}" \
    "${EAVMLIB}"

info "Packed: ${WORK_DIR}/app.avm"
"${PACKBEAM}" -l "${WORK_DIR}/app.avm" | head -8
echo "  ..."

# -- Step 4: Copy AtomVM_http.wasm --------------------------------------------

cp "${ATOMVM_HTTP}" "${WORK_DIR}/AtomVM_http.wasm"

# -- Step 5: Write spin.toml --------------------------------------------------

cat > "${WORK_DIR}/spin.toml" << 'EOF'
spin_manifest_version = 2

[application]
name = "atomvm-erlang-http"
version = "0.1.0"
description = "Erlang HTTP handler running on AtomVM + Fermyon Spin"

[[trigger.http]]
route = "/..."
component = "atomvm"

[component.atomvm]
source = "AtomVM_http.wasm"
files = [{ source = "app.avm", destination = "/app.avm" }]
key_value_stores = ["default"]
allowed_outbound_hosts = ["https://*:*"]
EOF

info "Generated: ${WORK_DIR}/spin.toml"

# -- Step 6: Run ---------------------------------------------------------------

if $BUILD_ONLY; then
    info "Build complete. Files in: ${WORK_DIR}/"
    echo ""
    echo "To run manually:"
    echo "  cd ${WORK_DIR} && spin up --listen 127.0.0.1:${PORT}"
    exit 0
fi

command -v spin >/dev/null 2>&1 || error "spin not found. Install from https://spinframework.dev/"

info "Starting Spin on http://127.0.0.1:${PORT} ..."
echo ""
cd "${WORK_DIR}"
exec spin up --listen "127.0.0.1:${PORT}"
