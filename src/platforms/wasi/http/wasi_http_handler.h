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

#ifndef _WASI_HTTP_HANDLER_H_
#define _WASI_HTTP_HANDLER_H_

#include "globalcontext.h"

/**
 * @brief Initialize the HTTP handler subsystem.
 * @details Must be called after GlobalContext is created and modules loaded,
 * but before any HTTP requests arrive. Stores the global context for use
 * by the incoming-handler export.
 */
void wasi_http_handler_init(GlobalContext *glb);

/**
 * @brief Get the stored GlobalContext for the HTTP handler.
 * @returns The GlobalContext, or NULL if not initialized.
 */
GlobalContext *wasi_http_handler_get_global(void);

#endif
