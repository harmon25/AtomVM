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

/*
 * WASI sockets are always non-blocking, but fcntl(fd, F_SETFL, O_NONBLOCK)
 * returns an error since WASI doesn't support modifying socket flags this way.
 * Provide a wrapper that silently succeeds for F_SETFL + O_NONBLOCK.
 */
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <sys/socket.h>

static inline int wasi_fcntl(int fd, int cmd, ...)
{
    va_list ap;
    va_start(ap, cmd);
    int arg = va_arg(ap, int);
    va_end(ap);

    if (cmd == F_SETFL && (arg & O_NONBLOCK)) {
        /* WASI sockets are always non-blocking; silently succeed */
        return 0;
    }
    if (cmd == F_GETFL) {
        /* Return a reasonable default; O_NONBLOCK is set */
        return O_NONBLOCK;
    }
    /* For other commands, let the real fcntl handle it (will likely fail) */
    return -1;
}

#define fcntl wasi_fcntl

/*
 * WASI connect() on non-blocking sockets returns EINPROGRESS, but the
 * otp_socket.c code doesn't handle waiting for connection completion.
 * Provide a wrapper that blocks until the connection completes or fails.
 */
#include <sys/socket.h>
#include <netinet/in.h>

static inline int wasi_connect(int fd, const struct sockaddr *addr, socklen_t len)
{
    int res = connect(fd, addr, len);
    if (res < 0 && errno == EINPROGRESS) {
        /* Wait for connection to complete using poll */
        struct pollfd pfd = { .fd = fd, .events = POLLOUT };
        int poll_res = poll(&pfd, 1, 30000); /* 30 second timeout */
        if (poll_res > 0) {
            /* Check if connection succeeded */
            int so_error = 0;
            socklen_t so_len = sizeof(so_error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &so_len) == 0) {
                if (so_error == 0) {
                    /* Connection succeeded */
                    return 0;
                } else {
                    /* Connection failed, set errno */
                    errno = so_error;
                    return -1;
                }
            }
        } else if (poll_res == 0) {
            /* Timeout */
            errno = ETIMEDOUT;
            return -1;
        }
        /* Poll failed, return original error */
    }
    return res;
}

#define connect wasi_connect

#endif
