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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static void print_help(const char *program_name)
{
    printf(
        "\n"
        "Syntax:\n"
        "\n"
        "    %s [-h] [-v] <path-to-avm-or-beam-file>+\n"
        "\n"
        "Options:\n"
        "\n"
        "    -h         Print this help and exit.\n"
        "    -v         Print the AtomVM version and exit.\n"
        "\n"
        "Supply one or more AtomVM packbeam (.avm) files to start your application.\n"
        "\n"
        "Example:\n"
        "\n"
        "    $ wasmtime --dir=. %s /path/to/my/application.avm\n"
        "\n",
        program_name, program_name);
}

int main(int argc, char **argv)
{
    // Simple argument parsing (WASI supports argc/argv)
    // Note: getopt may not be available in all wasi-libc builds, so we
    // parse manually.
    int first_file_arg = 1;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-') {
            first_file_arg = i;
            break;
        }
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return EXIT_SUCCESS;
        }
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf(ATOMVM_VERSION "\n");
            return EXIT_SUCCESS;
        }
        // Unknown flag — skip and treat rest as files
        first_file_arg = i + 1;
    }

    if (first_file_arg >= argc) {
        fprintf(stderr, "Error: no .avm or .beam files specified.\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    GlobalContext *glb = globalcontext_new();

    Module *startup_module = NULL;

    for (int i = first_file_arg; i < argc; i++) {
        const char *ext = strrchr(argv[i], '.');
        if (ext && strcmp(ext, ".avm") == 0) {
            struct AVMPackData *avmpack_data;
            if (UNLIKELY(sys_open_avm_from_file(glb, argv[i], &avmpack_data) != AVM_OPEN_OK)) {
                fprintf(stderr, "Failed opening %s.\n", argv[i]);
                globalcontext_destroy(glb);
                return EXIT_FAILURE;
            }
            synclist_append(&glb->avmpack_data, &avmpack_data->avmpack_head);

            if (IS_NULL_PTR(startup_module)) {
                const void *startup_beam = NULL;
                const char *startup_module_name;
                uint32_t startup_beam_size;
                avmpack_find_section_by_flag(avmpack_data->data, BEAM_START_FLAG, BEAM_START_FLAG, &startup_beam, &startup_beam_size, &startup_module_name);

                if (startup_beam) {
                    avmpack_data->in_use = true;
                    startup_module = module_new_from_iff_binary(glb, startup_beam, startup_beam_size);
                    if (IS_NULL_PTR(startup_module)) {
                        fprintf(stderr, "Cannot load startup module: %s\n", startup_module_name);
                        globalcontext_destroy(glb);
                        return EXIT_FAILURE;
                    }
                    globalcontext_insert_module(glb, startup_module);
                    startup_module->module_platform_data = NULL;
                }
            }

        } else if (ext && strcmp(ext, ".beam") == 0) {
            Module *mod = sys_load_module_from_file(glb, argv[i]);
            if (IS_NULL_PTR(mod)) {
                fprintf(stderr, "Cannot load module: %s\n", argv[i]);
                globalcontext_destroy(glb);
                return EXIT_FAILURE;
            }
            globalcontext_insert_module(glb, mod);
            mod->module_platform_data = NULL;
            if (IS_NULL_PTR(startup_module) && module_search_exported_function(mod, START_ATOM_INDEX, 0) != 0) {
                startup_module = mod;
            }

        } else {
            fprintf(stderr, "%s is not an AVM or a BEAM file.\n", argv[i]);
            globalcontext_destroy(glb);
            return EXIT_FAILURE;
        }
    }

    if (IS_NULL_PTR(startup_module)) {
        fprintf(stderr, "Error: no startup module found.\n");
        globalcontext_destroy(glb);
        return EXIT_FAILURE;
    }

    run_result_t result = globalcontext_run(glb, startup_module, stderr, 0, NULL);

    int status;
    if (result == RUN_SUCCESS) {
        status = EXIT_SUCCESS;
    } else {
        status = EXIT_FAILURE;
    }

    globalcontext_destroy(glb);

    return status;
}
