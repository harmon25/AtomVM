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

/**
 * @brief WASI platform event listener type.
 * @details On WASI, there is no fd-based I/O multiplexing (no poll/select/epoll).
 * This type is defined to satisfy the listeners.h framework requirements but
 * is not actively used in the MVP.
 */
typedef int listener_event_t;

struct EventListener
{
    struct ListHead listeners_list_head;
    event_handler_t handler;
    int fd; // placeholder, not used in WASI MVP
};

/**
 * @brief WASI platform data.
 * @details Minimal platform state for the WASI target. No event loop fds,
 * no signal mechanism, no threading.
 */
struct WASIPlatformData
{
    int dummy; // Placeholder to avoid empty struct
};

#endif
