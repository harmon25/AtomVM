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

cmake_minimum_required(VERSION 3.13)
project(libAtomVMPlatformWASI)

set(HEADER_FILES
    wasi_sys.h
    otp_socket_platform.h
    platform_defaultatoms.def
    platform_defaultatoms.h
    ../../../libAtomVM/otp_net.h
    ../../../libAtomVM/otp_socket.h
)

set(SOURCE_FILES
    platform_defaultatoms.c
    platform_nifs.c
    otp_socket_platform.c
    sys.c
    ../../../libAtomVM/inet.c
    ../../../libAtomVM/otp_net.c
    ../../../libAtomVM/otp_socket.c
)

set(
    PLATFORM_LIB_SUFFIX
    ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}
)

add_library(libAtomVM${PLATFORM_LIB_SUFFIX} ${SOURCE_FILES} ${HEADER_FILES})
target_compile_features(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC c_std_11)
target_compile_options(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC
    -Wall -Wextra
    # wasi-libc implements lseek() as a macro using GNU statement expressions
    -Wno-gnu-statement-expression-from-macro-expansion
    # Force-include WASI compatibility shims for otp_socket.c, etc.
    -include "${CMAKE_CURRENT_SOURCE_DIR}/wasi_compat.h"
)

target_link_libraries(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC libAtomVM)
target_include_directories(libAtomVM${PLATFORM_LIB_SUFFIX} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

include(DefineIfExists)
define_if_function_exists(libAtomVM${PLATFORM_LIB_SUFFIX} signal "signal.h" PRIVATE HAVE_SIGNAL)
define_if_function_exists(libAtomVM${PLATFORM_LIB_SUFFIX} getservbyname "netdb.h" PRIVATE HAVE_SERVBYNAME)
define_if_function_exists(libAtomVM${PLATFORM_LIB_SUFFIX} gethostname "unistd.h" PRIVATE HAVE_GETHOSTNAME)
