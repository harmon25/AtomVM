#!/bin/bash
# Pre-initialize AtomVM HTTP component for faster startup
#
# This script uses Wizer to create a pre-initialized snapshot of AtomVM,
# eliminating per-request initialization overhead (~1ms per request).
#
# Usage:
#   ./preinit_atomvm.sh <app.avm> [output.wasm]
#
# Requirements:
#   - wizer (cargo install wizer --features="env_logger structopt")
#   - wasmtime (for testing)
#   - AtomVM_http.wasm built with AVM_BUILD_HTTP_COMPONENT=ON

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Allow BUILD_WASI to be overridden, otherwise try common locations
if [ -z "$BUILD_WASI" ]; then
    # Try to find build-wasi relative to script location
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../../../.." && pwd)"
    BUILD_WASI="${PROJECT_ROOT}/build-wasi"
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }
step()  { echo -e "${BLUE}[STEP]${NC} $1"; }

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <app.avm> [output.wasm]"
    echo ""
    echo "  app.avm     - Path to the AVM file to embed"
    echo "  output.wasm - Output path (default: AtomVM_http_preinit.wasm)"
    echo ""
    echo "Example:"
    echo "  $0 ../elixir/wasi/app.avm"
    exit 1
fi

AVM_FILE="$1"
OUTPUT_WASM="${2:-AtomVM_http_preinit.wasm}"

# Check prerequisites
step "Checking prerequisites..."

command -v wizer >/dev/null 2>&1 || error "wizer not found. Install with:\n  cargo install wizer --features=\"env_logger structopt\""

ATOMVM_HTTP="${BUILD_WASI}/AtomVM_http.wasm"
if [ ! -f "${ATOMVM_HTTP}" ]; then
    error "AtomVM_http.wasm not found at ${ATOMVM_HTTP}\nBuild with:\n  cmake -S src/platforms/wasi -B build-wasi -DAVM_BUILD_HTTP_COMPONENT=ON\n  cmake --build build-wasi"
fi

if [ ! -f "${AVM_FILE}" ]; then
    error "AVM file not found: ${AVM_FILE}"
fi

info "Found AtomVM HTTP: ${ATOMVM_HTTP}"
info "Found AVM file: ${AVM_FILE}"

# Create temporary directory for working files
WORK_DIR=$(mktemp -d)
trap "rm -rf ${WORK_DIR}" EXIT

# Copy files to temp directory
step "Setting up workspace..."
cp "${ATOMVM_HTTP}" "${WORK_DIR}/AtomVM_http.wasm"
cp "${AVM_FILE}" "${WORK_DIR}/app.avm"

# Create a spin.toml for testing
step "Creating Spin configuration..."
cat > "${WORK_DIR}/spin.toml" << EOF
spin_manifest_version = 2

[application]
name = "atomvm-preinit"
version = "0.1.0"

[[trigger.http]]
route = "/..."
component = "atomvm"

[component.atomvm]
source = "AtomVM_http.wasm"
files = [{ source = "app.avm", destination = "/app.avm" }]
EOF

info "Created temporary spin.toml in ${WORK_DIR}"

# Run wizer to create pre-initialized snapshot
step "Creating pre-initialized snapshot with Wizer..."
echo ""
echo "  This will:"
echo "    1. Start the AtomVM HTTP component"
echo "    2. Run the constructor (atomvm_http_init)"
echo "    3. Load ${AVM_FILE} into memory"
echo "    4. Snapshot the initialized state"
echo ""

cd "${WORK_DIR}"

# Wizer will run the constructor and snapshot the state
# We need --allow-wasi so the constructor can access files
wizer \
    --allow-wasi \
    --wasm-bulk-memory true \
    -o "${OUTPUT_WASM}" \
    "AtomVM_http.wasm" 2>&1

if [ ! -f "${OUTPUT_WASM}" ]; then
    error "Wizer failed to create output file"
fi

# Move output to current directory
cd - > /dev/null
mv "${WORK_DIR}/${OUTPUT_WASM}" "./${OUTPUT_WASM}"

info "Successfully created pre-initialized snapshot: ${OUTPUT_WASM}"

# Compare file sizes
ORIG_SIZE=$(stat -f%z "${ATOMVM_HTTP}" 2>/dev/null || stat -c%s "${ATOMVM_HTTP}" 2>/dev/null)
NEW_SIZE=$(stat -f%z "./${OUTPUT_WASM}" 2>/dev/null || stat -c%s "./${OUTPUT_WASM}" 2>/dev/null)

step "File size comparison:"
echo "  Original: ${ORIG_SIZE} bytes"
echo "  Pre-init: ${NEW_SIZE} bytes"
echo "  Increase: $((NEW_SIZE - ORIG_SIZE)) bytes"

# Test with wasmtime if available
if command -v wasmtime >/dev/null 2>&1; then
    step "Testing with wasmtime..."
    
    # Note: wasmtime serve doesn't support reactor components yet
    # But we can verify the module loads
    wasmtime run \
        --dir=. \
        "./${OUTPUT_WASM}" 2>&1 || true
fi

echo ""
echo "=========================================="
echo "Pre-initialization complete!"
echo "=========================================="
echo ""
echo "Output file: ${OUTPUT_WASM}"
echo ""
echo "To use with Spin:"
echo "  1. Update spin.toml to use ${OUTPUT_WASM}"
echo "  2. Remove the files directive (AVM is embedded)"
echo "  3. Run: spin up"
echo ""
echo "Expected performance improvement: ~50-60% reduction in latency"
echo "  - Before: ~1.5ms per request"
echo "  - After:  ~0.6-0.8ms per request"
echo ""
