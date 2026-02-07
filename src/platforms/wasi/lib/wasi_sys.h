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

#ifndef _WASI_SYS_H_
#define _WASI_SYS_H_

#include <list.h>
#include <sys.h>
#include <poll.h>

/**
 * @brief WASI platform event listener type.
 * @details On WASI p2, fds (including sockets) can be polled using poll(2).
 */
typedef int listener_event_t;

struct EventListener
{
    struct ListHead listeners_list_head;
    event_handler_t handler;
    int fd;
};

/**
 * @brief WASI platform data.
 * @details Holds the dynamic pollfd array used by sys_poll_events for
 * I/O multiplexing on sockets and listeners.
 */
struct WASIPlatformData
{
    struct pollfd *fds;             // dynamically allocated pollfd array
    int fds_count;                  // current allocation size
    int select_events_poll_count;   // -1 = dirty, rebuild needed
};

#endif
