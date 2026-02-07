/*
 * This file is part of AtomVM.
 *
 * Copyright 2025 AtomVM Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
 */

/**
 * @file wasi_compat.h
 * @brief Compatibility shims for functions missing from wasi-libc.
 *
 * @details This header is force-included (via -include) when compiling
 * libAtomVM for the WASI target. It provides stubs for POSIX functions
 * that wasi-libc declares but does not define, or excludes entirely.
 */

#ifndef _WASI_COMPAT_H_
#define _WASI_COMPAT_H_

/*
 * wasi-libc excludes tzset() behind __wasilibc_unmodified_upstream because
 * WASI has no timezone database. libAtomVM nifs.c calls tzset() for
 * erlang:localtime/0. Provide a no-op inline.
 */
static inline void tzset(void)
{
    /* No-op: WASI has no timezone database */
}

#endif
