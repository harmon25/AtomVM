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

#include "sys.h"
#include "wasi_sys.h"

#include "avmpack.h"
#include "defaultatoms.h"
#include "iff.h"
#include "otp_net.h"
#include "otp_socket.h"
#include "scheduler.h"
#include "utils.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

// listeners.h expects the platform to provide these two static functions
// before inclusion.  On WASI there is no polling set, so they are no-ops.
static void event_listener_add_to_polling_set(struct EventListener *listener, GlobalContext *glb)
{
    UNUSED(listener);
    UNUSED(glb);
}

static void listener_event_remove_from_polling_set(listener_event_t event, GlobalContext *glb)
{
    UNUSED(event);
    UNUSED(glb);
}

static bool event_listener_is_event(EventListener *listener, listener_event_t event)
{
    return listener->fd == event;
}

#include "listeners.h"

#include "resources.h"
#include "trace.h"

// ---------------------------------------------------------------------------
// File loading helpers (open/read/lseek/malloc, no mmap)
// ---------------------------------------------------------------------------

/**
 * @brief Read an entire file into a malloc'd buffer using POSIX open/read/lseek.
 * @param path file path
 * @param out_size if non-NULL, receives the file size
 * @return pointer to file data, or NULL on failure. Caller must free().
 */
static void *read_file_to_buffer(const char *path, size_t *out_size)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return NULL;
    }

    off_t fsize = lseek(fd, 0, SEEK_END);
    if (fsize < 0) {
        close(fd);
        return NULL;
    }
    lseek(fd, 0, SEEK_SET);

    void *data = malloc((size_t) fsize);
    if (IS_NULL_PTR(data)) {
        close(fd);
        return NULL;
    }

    size_t total_read = 0;
    while (total_read < (size_t) fsize) {
        ssize_t r = read(fd, (uint8_t *) data + total_read, (size_t) fsize - total_read);
        if (r <= 0) {
            free(data);
            close(fd);
            return NULL;
        }
        total_read += (size_t) r;
    }

    close(fd);

    if (out_size) {
        *out_size = (size_t) fsize;
    }
    return data;
}

// ---------------------------------------------------------------------------
// AVM pack management (malloc-backed, no mmap)
// ---------------------------------------------------------------------------

static void malloc_avm_pack_destructor(struct AVMPackData *obj, GlobalContext *global)
{
    UNUSED(global);
    // The data was malloc'd in sys_open_avm_from_file; free it.
    free((void *) obj->data);
    free(obj);
}

static const struct AVMPackInfo malloc_avm_pack_info = {
    .destructor = malloc_avm_pack_destructor
};

// ---------------------------------------------------------------------------
// sys.h required implementations
// ---------------------------------------------------------------------------

void sys_poll_events(GlobalContext *glb, int timeout_ms)
{
    struct WASIPlatformData *platform = glb->platform_data;

    // If there are no select events and no listeners, just sleep
    if (synclist_is_empty(&glb->select_events) && synclist_is_empty(&glb->listeners)) {
        if (timeout_ms > 0) {
            struct timespec ts;
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = (long) (timeout_ms % 1000) * 1000000L;
            nanosleep(&ts, NULL);
        }
        return;
    }

    // Rebuild the pollfd array if the select_events set is dirty
    int select_events_poll_count = platform->select_events_poll_count;
    struct pollfd *fds = platform->fds;
    int fd_index = 0;

    if (select_events_poll_count < 0) {
        struct ListHead *select_events = synclist_wrlock(&glb->select_events);
        size_t select_events_new_count;
        select_event_count_and_destroy_closed(select_events, NULL, NULL, &select_events_new_count, glb);

        fds = realloc(fds, sizeof(struct pollfd) * select_events_new_count);
        platform->fds = fds;

        struct ListHead *item;
        fd_index = 0;
        LIST_FOR_EACH (item, select_events) {
            struct SelectEvent *select_event = GET_LIST_ENTRY(item, struct SelectEvent, head);
            if (select_event->read || select_event->write) {
                fds[fd_index].fd = select_event->event;
                fds[fd_index].events = (select_event->read ? POLLIN : 0) | (select_event->write ? POLLOUT : 0);
                fds[fd_index].revents = 0;
                fd_index++;
            }
        }
        platform->select_events_poll_count = (int) select_events_new_count;
        select_events_poll_count = (int) select_events_new_count;
        synclist_unlock(&glb->select_events);
    }

    if (select_events_poll_count == 0) {
        // No fds to poll — just sleep
        if (timeout_ms > 0) {
            struct timespec ts;
            ts.tv_sec = timeout_ms / 1000;
            ts.tv_nsec = (long) (timeout_ms % 1000) * 1000000L;
            nanosleep(&ts, NULL);
        }
        return;
    }

    int nb_descriptors = poll(fds, (nfds_t) select_events_poll_count, timeout_ms);

    for (int i = 0; i < select_events_poll_count && nb_descriptors > 0; i++) {
        if (!(fds[i].revents & fds[i].events)) {
            continue;
        }
        bool is_read = fds[i].revents & POLLIN;
        bool is_write = fds[i].revents & POLLOUT;
        fds[i].revents = 0;
        nb_descriptors--;

        select_event_notify(fds[i].fd, is_read, is_write, glb);
    }
}

void sys_time(struct timespec *t)
{
    if (UNLIKELY(clock_gettime(CLOCK_REALTIME, t))) {
        fprintf(stderr, "Failed clock_gettime.\n");
        AVM_ABORT();
    }
}

void sys_monotonic_time(struct timespec *t)
{
    if (UNLIKELY(clock_gettime(CLOCK_MONOTONIC, t))) {
        fprintf(stderr, "Failed clock_gettime.\n");
        AVM_ABORT();
    }
}

uint64_t sys_monotonic_time_u64(void)
{
    struct timespec ts;
    sys_monotonic_time(&ts);
    // 2^64/10^9/86400/365 around 585 years
    return ((uint_least64_t) ts.tv_sec * 1000000000) + ts.tv_nsec;
}

uint64_t sys_monotonic_time_ms_to_u64(uint64_t ms)
{
    return ms * 1000000;
}

uint64_t sys_monotonic_time_u64_to_ms(uint64_t t)
{
    return t / 1000000;
}

enum OpenAVMResult sys_open_avm_from_file(
    GlobalContext *global, const char *path, struct AVMPackData **data)
{
    TRACE("sys_open_avm_from_file: Going to open: %s\n", path);
    UNUSED(global);

    size_t file_size = 0;
    void *file_data = read_file_to_buffer(path, &file_size);
    if (IS_NULL_PTR(file_data)) {
        return AVM_OPEN_CANNOT_OPEN;
    }

    if (UNLIKELY(!avmpack_is_valid(file_data, file_size))) {
        free(file_data);
        return AVM_OPEN_INVALID;
    }

    struct AVMPackData *avmpack_data = malloc(sizeof(struct AVMPackData));
    if (IS_NULL_PTR(avmpack_data)) {
        free(file_data);
        return AVM_OPEN_FAILED_ALLOC;
    }
    avmpack_data_init(avmpack_data, &malloc_avm_pack_info);
    avmpack_data->data = file_data;

    *data = avmpack_data;
    return AVM_OPEN_OK;
}

Module *sys_load_module_from_file(GlobalContext *global, const char *path)
{
    TRACE("sys_load_module_from_file: Going to load: %s\n", path);

    size_t file_size = 0;
    void *file_data = read_file_to_buffer(path, &file_size);
    if (IS_NULL_PTR(file_data)) {
        return NULL;
    }

    if (UNLIKELY(!iff_is_valid_beam(file_data))) {
        fprintf(stderr, "%s is not a valid BEAM file.\n", path);
        free(file_data);
        return NULL;
    }

    Module *new_module = module_new_from_iff_binary(global, file_data, file_size);
    if (IS_NULL_PTR(new_module)) {
        free(file_data);
        return NULL;
    }
    new_module->module_platform_data = NULL;

    return new_module;
}

Context *sys_create_port(GlobalContext *glb, const char *driver_name, term opts)
{
    UNUSED(glb);
    UNUSED(driver_name);
    UNUSED(opts);
    return NULL;
}

term sys_get_info(Context *ctx, term key)
{
    UNUSED(ctx);
    UNUSED(key);
    return UNDEFINED_ATOM;
}

void sys_init_platform(GlobalContext *global)
{
    struct WASIPlatformData *platform = malloc(sizeof(struct WASIPlatformData));
    if (UNLIKELY(!platform)) {
        AVM_ABORT();
    }
    platform->fds = NULL;
    platform->fds_count = 0;
    platform->select_events_poll_count = -1;
    global->platform_data = platform;

    otp_net_init(global);
    otp_socket_init(global);
}

void sys_free_platform(GlobalContext *global)
{
    struct WASIPlatformData *platform = global->platform_data;
    free(platform->fds);
    free(platform);
}

// ---------------------------------------------------------------------------
// Listener / select event stubs (no-ops on WASI)
// ---------------------------------------------------------------------------

void sys_register_listener(GlobalContext *global, struct EventListener *listener)
{
    UNUSED(global);
    UNUSED(listener);
}

void sys_unregister_listener(GlobalContext *global, struct EventListener *listener)
{
    UNUSED(global);
    UNUSED(listener);
}

// sys_listener_destroy is provided by listeners.h (included above)

void sys_register_select_event(GlobalContext *global, ErlNifEvent event, bool is_write)
{
    UNUSED(event);
    UNUSED(is_write);
    struct WASIPlatformData *platform = global->platform_data;
    platform->select_events_poll_count = -1; // mark dirty, rebuild on next poll
}

void sys_unregister_select_event(GlobalContext *global, ErlNifEvent event, bool is_write)
{
    UNUSED(event);
    UNUSED(is_write);
    struct WASIPlatformData *platform = global->platform_data;
    platform->select_events_poll_count = -1; // mark dirty, rebuild on next poll
}

// ---------------------------------------------------------------------------
// JIT stubs (JIT is disabled on WASI, but sys.h declares these unconditionally
// in newer AtomVM. Provide stubs to satisfy the linker.)
// ---------------------------------------------------------------------------

#ifdef AVM_NO_JIT
ModuleNativeEntryPoint sys_map_native_code(const uint8_t *native_code, size_t size, size_t offset)
{
    UNUSED(native_code);
    UNUSED(size);
    UNUSED(offset);
    return NULL;
}

bool sys_get_cache_native_code(GlobalContext *global, Module *mod, uint16_t *version, ModuleNativeEntryPoint *entry_point, uint32_t *labels)
{
    UNUSED(global);
    UNUSED(mod);
    UNUSED(version);
    UNUSED(entry_point);
    UNUSED(labels);
    return false;
}

void sys_set_cache_native_code(GlobalContext *global, Module *mod, uint16_t version, ModuleNativeEntryPoint entry_point, uint32_t labels)
{
    UNUSED(global);
    UNUSED(mod);
    UNUSED(version);
    UNUSED(entry_point);
    UNUSED(labels);
}
#endif
