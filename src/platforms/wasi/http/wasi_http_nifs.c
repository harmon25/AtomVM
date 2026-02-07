/*
 * This file is part of AtomVM.
 *
 * Copyright 2025 AtomVM Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * ...
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
 */

/**
 * @file wasi_http_nifs.c
 * @brief Strong override of wasi_http_nifs_get_nif for the HTTP component.
 *
 * This file is compiled only into AtomVM_http.wasm. It provides the strong
 * definition that overrides the weak no-op in platform_nifs.c, routing
 * spin_http:* NIF lookups to the outbound HTTP implementation.
 */

#include "wasi_http_outbound.h"
#include "nifs.h"

// Strong override: called by platform_nifs_get_nif in the platform lib
const struct Nif *wasi_http_nifs_get_nif(const char *nifname)
{
    return wasi_http_outbound_get_nif(nifname);
}
