#!/bin/bash
# Benchmark: AtomVM/Elixir vs Rust on Fermyon Spin
#
# Runs oha against both implementations and prints a comparison table.
#
# Prerequisites:
#   - oha:  cargo install oha
#   - spin: https://spinframework.dev/
#   - AtomVM Elixir handler built: examples/elixir/wasi/build_and_run.sh --build-only
#   - Rust handler built: cd benchmarks/rust-baseline && spin build
#
# Usage:
#   ./bench.sh                    # full benchmark (10s per test, 1 connection)
#   ./bench.sh --duration 5       # 5 seconds per test
#   ./bench.sh --connections 4    # 4 concurrent connections
#   ./bench.sh --routes hello     # only benchmark /hello

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ATOMVM_DIR="${PROJECT_ROOT}/build-wasi/test"
RUST_DIR="${SCRIPT_DIR}/rust-baseline"

ATOMVM_PORT=3030
RUST_PORT=3031

DURATION=10
CONNECTIONS=1
ROUTES="hello json compute"

# -- Parse arguments -----------------------------------------------------------

while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration|-d)  DURATION="$2"; shift 2 ;;
        --connections|-c) CONNECTIONS="$2"; shift 2 ;;
        --routes|-r)    ROUTES="$2"; shift 2 ;;
        --help|-h)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --duration N      Seconds per test (default: 10)"
            echo "  -c, --connections N   Concurrent connections (default: 1)"
            echo "  -r, --routes LIST     Space-separated routes to test (default: hello json compute)"
            exit 0
            ;;
        *) echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

# -- Helpers -------------------------------------------------------------------

BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${GREEN}>>>${NC} $1"; }
header(){ echo -e "\n${BOLD}${CYAN}$1${NC}"; }
die()   { echo -e "${RED}ERROR:${NC} $1" >&2; exit 1; }

# Extract a numeric value from oha text output.
# oha lines look like: "  Average:\t1.5713 ms" or "  Requests/sec:\t633.5461"
extract_oha() {
    grep "$1" | head -1 | awk '{for(i=1;i<=NF;i++) if($i ~ /^[0-9]/) {print $i; exit}}'
}

# -- Check prerequisites ------------------------------------------------------

command -v oha  >/dev/null 2>&1 || die "oha not found. Install: cargo install oha"
command -v spin >/dev/null 2>&1 || die "spin not found. Install from https://spinframework.dev/"

[ -f "${ATOMVM_DIR}/spin.toml" ] || die "AtomVM handler not built.\n  Run: examples/elixir/wasi/build_and_run.sh --build-only"
[ -f "${ATOMVM_DIR}/app.avm" ]   || die "app.avm not found in ${ATOMVM_DIR}"
[ -f "${RUST_DIR}/spin.toml" ]   || die "Rust handler not found.\n  Run: cd benchmarks/rust-baseline && spin build"

RUST_WASM="${RUST_DIR}/target/wasm32-wasip1/release/rust_baseline.wasm"
[ -f "${RUST_WASM}" ] || die "Rust WASM not built.\n  Run: cd benchmarks/rust-baseline && spin build"

# -- Start servers -------------------------------------------------------------

cleanup() {
    kill "${ATOMVM_PID}" "${RUST_PID}" 2>/dev/null
    wait "${ATOMVM_PID}" "${RUST_PID}" 2>/dev/null
}
trap cleanup EXIT

# Kill any existing spin on our ports
pkill -f "spin up" 2>/dev/null; true
sleep 2

info "Starting AtomVM/Elixir on :${ATOMVM_PORT}..."
(cd "${ATOMVM_DIR}" && exec spin up --listen "127.0.0.1:${ATOMVM_PORT}") </dev/null >/dev/null 2>&1 &
ATOMVM_PID=$!

info "Starting Rust on :${RUST_PORT}..."
(cd "${RUST_DIR}" && exec spin up --listen "127.0.0.1:${RUST_PORT}") </dev/null >/dev/null 2>&1 &
RUST_PID=$!

info "Waiting for servers..."
sleep 5

# Verify
curl -sf --max-time 3 "http://127.0.0.1:${ATOMVM_PORT}/hello" >/dev/null \
    || die "AtomVM not responding on :${ATOMVM_PORT}"
curl -sf --max-time 3 "http://127.0.0.1:${RUST_PORT}/hello" >/dev/null \
    || die "Rust not responding on :${RUST_PORT}"

info "Both servers ready."

# -- Run benchmarks ------------------------------------------------------------

RESULTS_DIR=$(mktemp -d)

header "Benchmark: AtomVM/Elixir vs Rust on Fermyon Spin"
echo ""
echo "  Duration:     ${DURATION}s per test"
echo "  Connections:  ${CONNECTIONS}"
echo "  Routes:       ${ROUTES}"
echo "  oha version:  $(oha --version 2>&1)"
echo ""

# Collect results into arrays for the summary table
declare -A DATA

for route in ${ROUTES}; do
    header "/${route}"

    for runtime in atomvm rust; do
        if [ "$runtime" = "atomvm" ]; then
            port="${ATOMVM_PORT}"
            label="AtomVM/Elixir"
        else
            port="${RUST_PORT}"
            label="Rust"
        fi

        url="http://127.0.0.1:${port}/${route}"

        # Warmup
        for _ in 1 2 3; do curl -sf --max-time 3 "$url" >/dev/null 2>&1; done

        info "  ${label}..."
        output=$(oha -z "${DURATION}s" -c "${CONNECTIONS}" --no-tui "$url" 2>&1)
        echo "$output" > "${RESULTS_DIR}/${runtime}_${route}.txt"

        rps=$(echo "$output" | extract_oha "Requests/sec")
        avg=$(echo "$output" | extract_oha "Average")
        fast=$(echo "$output" | extract_oha "Fastest")
        slow=$(echo "$output" | extract_oha "Slowest")

        DATA["${runtime}_${route}_rps"]="$rps"
        DATA["${runtime}_${route}_avg"]="$avg"
        DATA["${runtime}_${route}_fast"]="$fast"
        DATA["${runtime}_${route}_slow"]="$slow"

        echo "         Req/s: ${rps}  Avg: ${avg} ms  Fast: ${fast} ms  Slow: ${slow} ms"
    done
    echo ""
done

# -- Summary table -------------------------------------------------------------

header "Summary"
echo ""

printf "${BOLD}%-12s  %-10s  %12s  %12s  %12s  %12s${NC}\n" \
    "Route" "Runtime" "Req/s" "Avg" "Fastest" "Slowest"
printf "%-12s  %-10s  %12s  %12s  %12s  %12s\n" \
    "────────────" "──────────" "────────────" "────────────" "────────────" "────────────"

for route in ${ROUTES}; do
    for runtime in atomvm rust; do
        label=$([ "$runtime" = "atomvm" ] && echo "AtomVM" || echo "Rust")
        printf "%-12s  %-10s  %12s  %10s ms  %10s ms  %10s ms\n" \
            "/${route}" "$label" \
            "${DATA[${runtime}_${route}_rps]}" \
            "${DATA[${runtime}_${route}_avg]}" \
            "${DATA[${runtime}_${route}_fast]}" \
            "${DATA[${runtime}_${route}_slow]}"
    done
    printf "%-12s  %-10s  %12s  %12s  %12s  %12s\n" \
        "" "" "" "" "" ""
done

# -- Ratio table ---------------------------------------------------------------

header "Comparison (Rust is Nx faster)"
echo ""

printf "${BOLD}%-12s  %14s  %14s${NC}\n" "Route" "Throughput" "Avg Latency"
printf "%-12s  %14s  %14s\n" "────────────" "──────────────" "──────────────"

for route in ${ROUTES}; do
    a_rps="${DATA[atomvm_${route}_rps]}"
    r_rps="${DATA[rust_${route}_rps]}"
    a_lat="${DATA[atomvm_${route}_avg]}"
    r_lat="${DATA[rust_${route}_avg]}"
    python3 -c "
a_rps,r_rps,a_lat,r_lat = $a_rps,$r_rps,$a_lat,$r_lat
rps_r = r_rps/a_rps if a_rps else 0
lat_r = a_lat/r_lat if r_lat else 0
print(f'{rps_r:>12.1f}x  {lat_r:>12.1f}x')
" 2>/dev/null | while read line; do printf "%-12s  %s\n" "/${route}" "$line"; done
done

echo ""
info "Raw oha output saved to: ${RESULTS_DIR}/"
info "Done."
