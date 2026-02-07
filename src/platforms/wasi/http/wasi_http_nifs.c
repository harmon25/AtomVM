/*
 * This file is part of AtomVM.
 *
 * Copyright 2025 AtomVM Contributors
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
 */

/**
 * @file wasi_http_nifs.c
 * @brief Strong override of wasi_http_nifs_get_nif for the HTTP component.
 *
 * Routes NIF lookups to:
 *   - spin_http:*    (outbound HTTP)
 *   - spin_kv:*      (key-value store)
 *   - spin_sqlite:*  (SQLite database)
 *   - spin_postgres:*(PostgreSQL database)
 */

#include "wasi_http_outbound.h"
#include "nifs.h"

// Defined in wasi_spin_nifs.c
extern const struct Nif *wasi_spin_nifs_get_nif(const char *nifname);

// Strong override: called by platform_nifs_get_nif in the platform lib
const struct Nif *wasi_http_nifs_get_nif(const char *nifname)
{
    const struct Nif *nif = wasi_http_outbound_get_nif(nifname);
    if (nif) return nif;

    nif = wasi_spin_nifs_get_nif(nifname);
    return nif;
}
