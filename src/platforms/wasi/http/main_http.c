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
 * @file main_http.c
 * @brief Reactor-mode entry point for AtomVM HTTP component.
 *
 * In the WASI component model, a "reactor" is initialized once via
 * __wasi_reactor_startup() (or the constructor), and then the host
 * invokes exported functions on demand. For HTTP, the host calls
 * wasi:http/incoming-handler.handle for each request.
 *
 * This file provides the initialization that:
 * 1. Creates the AtomVM GlobalContext
 * 2. Loads the application .avm file from known paths
 * 3. Stores the GlobalContext for use by the HTTP handler
 *
 * The actual request handling is in wasi_http_handler.c.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atom.h"
#include "avm_version.h"
#include "avmpack.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "iff.h"
#include "module.h"
#include "sys.h"
#include "term.h"
#include "utils.h"

#include "wasi_http_handler.h"

// ---------------------------------------------------------------------------
// AVM file search paths
// ---------------------------------------------------------------------------

static const char *avm_search_paths[] = {
    "/app.avm",
    "/spin_handler.avm",
    "./app.avm",
    "./spin_handler.avm",
    NULL
};

// ---------------------------------------------------------------------------
// Reactor initialization
// ---------------------------------------------------------------------------

static bool g_initialized = false;

/**
 * @brief Initialize the AtomVM runtime and load the application.
 *
 * This is called automatically by the WASI runtime before any exported
 * functions are invoked. It's marked as a constructor to ensure it runs
 * during module instantiation.
 */
__attribute__((constructor))
static void atomvm_http_init(void)
{
    if (g_initialized) {
        return;
    }

    // Find the AVM file
    const char *avm_path = NULL;
    for (const char **path = avm_search_paths; *path != NULL; path++) {
        if (access(*path, F_OK) == 0) {
            avm_path = *path;
            break;
        }
    }

    if (!avm_path) {
        fprintf(stderr, "AtomVM HTTP: No .avm file found. Searched:\n");
        for (const char **path = avm_search_paths; *path != NULL; path++) {
            fprintf(stderr, "  %s\n", *path);
        }
        fprintf(stderr, "Please provide an app.avm file.\n");
        // Don't abort — let the handler return 500 errors
        g_initialized = true;
        return;
    }

    fprintf(stderr, "AtomVM HTTP: Loading %s\n", avm_path);

    // Create the AtomVM global context
    GlobalContext *glb = globalcontext_new();
    if (!glb) {
        fprintf(stderr, "AtomVM HTTP: Failed to create GlobalContext\n");
        g_initialized = true;
        return;
    }

    // Load the AVM pack
    struct AVMPackData *avmpack_data;
    if (UNLIKELY(sys_open_avm_from_file(glb, avm_path, &avmpack_data) != AVM_OPEN_OK)) {
        fprintf(stderr, "AtomVM HTTP: Failed to open %s\n", avm_path);
        globalcontext_destroy(glb);
        g_initialized = true;
        return;
    }
    synclist_append(&glb->avmpack_data, &avmpack_data->avmpack_head);

    // Find and load the startup module (so module table is populated)
    const void *startup_beam = NULL;
    const char *startup_module_name = NULL;
    uint32_t startup_beam_size;
    avmpack_find_section_by_flag(
        avmpack_data->data, BEAM_START_FLAG, BEAM_START_FLAG,
        &startup_beam, &startup_beam_size, &startup_module_name);

    if (startup_beam) {
        avmpack_data->in_use = true;
        Module *startup_module = module_new_from_iff_binary(glb, startup_beam, startup_beam_size);
        if (startup_module) {
            globalcontext_insert_module(glb, startup_module);
            startup_module->module_platform_data = NULL;
            fprintf(stderr, "AtomVM HTTP: Loaded startup module: %s\n",
                startup_module_name ? startup_module_name : "(unknown)");
        }
    }

    // Store the global context for the HTTP handler
    wasi_http_handler_init(glb);

    fprintf(stderr, "AtomVM HTTP: Reactor initialized, ready for requests\n");
    g_initialized = true;
}
