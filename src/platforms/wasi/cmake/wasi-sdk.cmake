#
# This file is part of AtomVM.
#
# Copyright 2025 AtomVM Contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#    http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
#

# CMake toolchain file for building AtomVM with wasi-sdk.
#
# Usage:
#   export WASI_SDK_PATH=/opt/wasi-sdk-25.0
#   cmake -S src/platforms/wasi -B build-wasi \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake
#
# Or pass WASI_SDK_PATH as a CMake variable:
#   cmake -S src/platforms/wasi -B build-wasi \
#     -DWASI_SDK_PATH=/opt/wasi-sdk-25.0 \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/wasi-sdk.cmake

if(NOT DEFINED WASI_SDK_PATH)
    if(DEFINED ENV{WASI_SDK_PATH})
        set(WASI_SDK_PATH "$ENV{WASI_SDK_PATH}")
    else()
        set(WASI_SDK_PATH "/opt/wasi-sdk")
    endif()
endif()

if(NOT EXISTS "${WASI_SDK_PATH}/bin/clang")
    message(FATAL_ERROR
        "wasi-sdk not found at '${WASI_SDK_PATH}'.\n"
        "Please set WASI_SDK_PATH to the root of your wasi-sdk installation.\n"
        "Download wasi-sdk from: https://github.com/WebAssembly/wasi-sdk/releases"
    )
endif()

set(CMAKE_SYSTEM_NAME WASI)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR wasm32)

set(CMAKE_C_COMPILER "${WASI_SDK_PATH}/bin/clang")
set(CMAKE_CXX_COMPILER "${WASI_SDK_PATH}/bin/clang++")
set(CMAKE_AR "${WASI_SDK_PATH}/bin/llvm-ar")
set(CMAKE_RANLIB "${WASI_SDK_PATH}/bin/llvm-ranlib")
set(CMAKE_C_COMPILER_TARGET "wasm32-wasip1")
set(CMAKE_CXX_COMPILER_TARGET "wasm32-wasip1")
set(CMAKE_SYSROOT "${WASI_SDK_PATH}/share/wasi-sysroot")

# wasm32-wasi cannot produce executables for try_compile by default.
# Use STATIC_LIBRARY mode so CMake feature detection works.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
