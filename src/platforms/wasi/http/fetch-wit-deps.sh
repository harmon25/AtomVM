#!/bin/bash
# Populate wit/deps/ from the vendored deps.bak/ reference copy.
#
# The deps.bak/ directory contains full upstream clones of wasi-http
# and its transitive WASI dependencies. This script reorganises them
# into the flat layout that wit-bindgen expects:
#
#   wit/deps/http/     ← wasi:http@0.2.8
#   wit/deps/io/       ← wasi:io@0.2.8
#   wit/deps/cli/      ← wasi:cli@0.2.8
#   wit/deps/clocks/   ← wasi:clocks@0.2.8
#   ...
#
# Usage:
#   ./fetch-wit-deps.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WIT_DIR="${SCRIPT_DIR}/wit"
DEPS_BAK="${WIT_DIR}/deps.bak"
DEPS_OUT="${WIT_DIR}/deps"

if [ ! -d "${DEPS_BAK}" ]; then
    echo "ERROR: deps.bak/ not found at ${DEPS_BAK}" >&2
    echo "This directory should contain the upstream wasi-http repo." >&2
    exit 1
fi

if [ -d "${DEPS_OUT}" ]; then
    echo "wit/deps/ already exists — removing and recreating."
    rm -rf "${DEPS_OUT}"
fi

mkdir -p "${DEPS_OUT}"

# Copy the wasi:http WIT files into deps/http/
mkdir -p "${DEPS_OUT}/http"
cp "${DEPS_BAK}/wasi-http/wit/"*.wit "${DEPS_OUT}/http/"

# Copy transitive dependencies (io, cli, clocks, etc.)
# These live under wasi-http/wit/deps/ in the upstream repo.
for dep_dir in "${DEPS_BAK}/wasi-http/wit/deps/"*/; do
    dep_name="$(basename "${dep_dir}")"
    cp -r "${dep_dir}" "${DEPS_OUT}/${dep_name}"
done

# Copy Spin-specific interfaces (fermyon:spin@2.0.0)
if [ -d "${DEPS_BAK}/spin" ]; then
    mkdir -p "${DEPS_OUT}/spin"
    cp "${DEPS_BAK}/spin/"*.wit "${DEPS_OUT}/spin/"
fi

echo "WIT dependencies populated in ${DEPS_OUT}/"
ls -1d "${DEPS_OUT}"/*/
