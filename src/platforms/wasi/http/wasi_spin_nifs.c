/*
 * This file is part of AtomVM.
 *
 * Copyright 2025 AtomVM Contributors
 * SPDX-License-Identifier: Apache-2.0 OR LGPL-2.1-or-later
 */

/**
 * @file wasi_spin_nifs.c
 * @brief NIF implementations for Spin platform interfaces:
 *   - spin_kv:open/1, get/2, set/3, delete/2, exists/2, get_keys/1, close/1
 *   - spin_sqlite:open/1, execute/2, execute/3, close/1
 *   - spin_postgres:open/1, query/2, query/3, execute/2, execute/3, close/1
 */

#include "generated/app.h"
#include "nifs.h"
#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "memory.h"
#include "term.h"
#include "utils.h"

#include <string.h>
#include <stdio.h>

// ===========================================================================
// Key-Value Store NIFs
// ===========================================================================

// Store handles are kept as opaque integers in Erlang terms.
// The WASI resource handle is an int32_t under the hood.

static term nif_spin_kv_open(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_binary(argv[0])) { RAISE_ERROR(BADARG_ATOM); }

    app_string_t label;
    app_string_dup_n(&label, term_binary_data(argv[0]), term_binary_size(argv[0]));

    fermyon_spin_key_value_own_store_t store;
    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_static_store_open(&label, &store, &err);
    app_string_free(&label);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        term reason = globalcontext_make_atom(glb, ATOM_STR("\xD", "no_such_store"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, reason);
        return t;
    }

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, term_from_int(store.__handle));
    return t;
}

static term nif_spin_kv_get(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_borrow_store_t borrow = fermyon_spin_key_value_borrow_store(store);

    app_string_t key;
    app_string_dup_n(&key, term_binary_data(argv[1]), term_binary_size(argv[1]));

    app_option_list_u8_t val;
    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_method_store_get(borrow, &key, &val, &err);
    app_string_free(&key);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        term reason = globalcontext_make_atom(glb, ATOM_STR("\xA", "kv_error"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, reason);
        return t;
    }

    if (!val.is_some) {
        // Key not found → {ok, undefined}
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, OK_ATOM);
        term_put_tuple_element(t, 1, UNDEFINED_ATOM);
        return t;
    }

    size_t heap_needed = TUPLE_SIZE(2) + term_binary_heap_size(val.val.len);
    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        app_list_u8_free(&val.val);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term bin = term_from_literal_binary(val.val.ptr, val.val.len, &ctx->heap, glb);
    app_list_u8_free(&val.val);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, bin);
    return t;
}

static term nif_spin_kv_set(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1]) || !term_is_binary(argv[2])) {
        RAISE_ERROR(BADARG_ATOM);
    }

    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_borrow_store_t borrow = fermyon_spin_key_value_borrow_store(store);

    app_string_t key;
    app_string_dup_n(&key, term_binary_data(argv[1]), term_binary_size(argv[1]));

    app_list_u8_t value = {
        .ptr = (uint8_t *) term_binary_data(argv[2]),
        .len = term_binary_size(argv[2])
    };

    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_method_store_set(borrow, &key, &value, &err);
    app_string_free(&key);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        term reason = globalcontext_make_atom(glb, ATOM_STR("\xA", "kv_error"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, reason);
        return t;
    }

    return OK_ATOM;
}

static term nif_spin_kv_delete(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_borrow_store_t borrow = fermyon_spin_key_value_borrow_store(store);

    app_string_t key;
    app_string_dup_n(&key, term_binary_data(argv[1]), term_binary_size(argv[1]));

    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_method_store_delete(borrow, &key, &err);
    app_string_free(&key);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        return ERROR_ATOM;
    }
    return OK_ATOM;
}

static term nif_spin_kv_exists(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_borrow_store_t borrow = fermyon_spin_key_value_borrow_store(store);

    app_string_t key;
    app_string_dup_n(&key, term_binary_data(argv[1]), term_binary_size(argv[1]));

    bool exists;
    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_method_store_exists(borrow, &key, &exists, &err);
    app_string_free(&key);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        return ERROR_ATOM;
    }
    return exists ? TRUE_ATOM : FALSE_ATOM;
}

static term nif_spin_kv_get_keys(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_borrow_store_t borrow = fermyon_spin_key_value_borrow_store(store);

    app_list_string_t keys;
    fermyon_spin_key_value_error_t err;
    bool ok = fermyon_spin_key_value_method_store_get_keys(borrow, &keys, &err);

    if (!ok) {
        fermyon_spin_key_value_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xA", "kv_error")));
        return t;
    }

    // Build list of binaries
    size_t heap_needed = TUPLE_SIZE(2);
    for (size_t i = 0; i < keys.len; i++) {
        heap_needed += term_binary_heap_size(keys.ptr[i].len) + CONS_SIZE;
    }
    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        app_list_string_free(&keys);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term list = term_nil();
    for (int i = (int) keys.len - 1; i >= 0; i--) {
        term bin = term_from_literal_binary(keys.ptr[i].ptr, keys.ptr[i].len, &ctx->heap, glb);
        list = term_list_prepend(bin, list, &ctx->heap);
    }
    app_list_string_free(&keys);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, list);
    return t;
}

static term nif_spin_kv_close(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    if (!term_is_integer(argv[0])) { RAISE_ERROR(BADARG_ATOM); }
    fermyon_spin_key_value_own_store_t store = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_key_value_store_drop_own(store);
    return OK_ATOM;
}

// ===========================================================================
// SQLite NIFs
// ===========================================================================

// Helper: convert SQLite value variant -> Erlang term
static term sqlite_value_to_term(Context *ctx, GlobalContext *glb, fermyon_spin_sqlite_value_t *val)
{
    switch (val->tag) {
        case FERMYON_SPIN_SQLITE_VALUE_INTEGER:
            return term_from_int64(val->val.integer);
        case FERMYON_SPIN_SQLITE_VALUE_REAL: {
            if (UNLIKELY(memory_ensure_free(ctx, FLOAT_SIZE) != MEMORY_GC_OK)) { return UNDEFINED_ATOM; }
            return term_from_float(val->val.real, &ctx->heap);
        }
        case FERMYON_SPIN_SQLITE_VALUE_TEXT:
            return term_from_literal_binary(val->val.text.ptr, val->val.text.len, &ctx->heap, glb);
        case FERMYON_SPIN_SQLITE_VALUE_BLOB:
            return term_from_literal_binary(val->val.blob.ptr, val->val.blob.len, &ctx->heap, glb);
        case FERMYON_SPIN_SQLITE_VALUE_NULL:
        default:
            return globalcontext_make_atom(glb, ATOM_STR("\x4", "null"));
    }
}

// Helper: convert Erlang term -> SQLite value parameter
static bool term_to_sqlite_value(Context *ctx, GlobalContext *glb, term t, fermyon_spin_sqlite_value_t *val)
{
    UNUSED(ctx);
    if (term_is_integer(t)) {
        val->tag = FERMYON_SPIN_SQLITE_VALUE_INTEGER;
        val->val.integer = term_to_int(t);
        return true;
    }
    if (term_is_float(t)) {
        val->tag = FERMYON_SPIN_SQLITE_VALUE_REAL;
        val->val.real = term_to_float(t);
        return true;
    }
    if (term_is_binary(t)) {
        val->tag = FERMYON_SPIN_SQLITE_VALUE_TEXT;
        app_string_dup_n(&val->val.text, term_binary_data(t), term_binary_size(t));
        return true;
    }
    term null_atom = globalcontext_make_atom(glb, ATOM_STR("\x4", "null"));
    if (t == null_atom || t == UNDEFINED_ATOM) {
        val->tag = FERMYON_SPIN_SQLITE_VALUE_NULL;
        return true;
    }
    return false;
}

static term nif_spin_sqlite_open(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_binary(argv[0])) { RAISE_ERROR(BADARG_ATOM); }

    app_string_t db_name;
    app_string_dup_n(&db_name, term_binary_data(argv[0]), term_binary_size(argv[0]));

    fermyon_spin_sqlite_own_connection_t conn;
    fermyon_spin_sqlite_error_t err;
    bool ok = fermyon_spin_sqlite_static_connection_open(&db_name, &conn, &err);
    app_string_free(&db_name);

    if (!ok) {
        fermyon_spin_sqlite_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\x10", "no_such_database")));
        return t;
    }

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, term_from_int(conn.__handle));
    return t;
}

static term nif_spin_sqlite_execute(Context *ctx, int argc, term argv[])
{
    GlobalContext *glb = ctx->global;

    // argv[0] = conn handle, argv[1] = SQL string, argv[2] = params (optional)
    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_sqlite_own_connection_t conn = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_sqlite_borrow_connection_t borrow = fermyon_spin_sqlite_borrow_connection(conn);

    app_string_t sql;
    app_string_dup_n(&sql, term_binary_data(argv[1]), term_binary_size(argv[1]));

    // Build parameters list
    fermyon_spin_sqlite_list_value_t params = { .ptr = NULL, .len = 0 };
    if (argc >= 3 && term_is_nonempty_list(argv[2])) {
        // Count params
        size_t count = 0;
        term cursor = argv[2];
        while (term_is_nonempty_list(cursor)) { count++; cursor = term_get_list_tail(cursor); }

        params.ptr = malloc(count * sizeof(fermyon_spin_sqlite_value_t));
        params.len = count;

        cursor = argv[2];
        for (size_t i = 0; i < count; i++) {
            term elem = term_get_list_head(cursor);
            if (!term_to_sqlite_value(ctx, glb, elem, &params.ptr[i])) {
                // Cleanup on failure
                for (size_t j = 0; j < i; j++) {
                    fermyon_spin_sqlite_value_free(&params.ptr[j]);
                }
                free(params.ptr);
                app_string_free(&sql);
                RAISE_ERROR(BADARG_ATOM);
            }
            cursor = term_get_list_tail(cursor);
        }
    }

    fermyon_spin_sqlite_query_result_t result;
    fermyon_spin_sqlite_error_t err;
    bool ok = fermyon_spin_sqlite_method_connection_execute(borrow, &sql, &params, &result, &err);
    app_string_free(&sql);

    // Free params
    for (size_t i = 0; i < params.len; i++) {
        fermyon_spin_sqlite_value_free(&params.ptr[i]);
    }
    free(params.ptr);

    if (!ok) {
        const char *msg = "sqlite_error";
        if (err.tag == FERMYON_SPIN_SQLITE_ERROR_IO && err.val.io.len > 0) {
            // Use the io error message
        }
        fermyon_spin_sqlite_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xC", "sqlite_error")));
        return t;
    }

    // Build result: {ok, #{columns => [binary()], rows => [[term()]]}}
    // Calculate heap needed
    size_t heap_needed = TUPLE_SIZE(2) + TERM_MAP_SIZE(2) + 16;

    // Column names
    for (size_t i = 0; i < result.columns.len; i++) {
        heap_needed += term_binary_heap_size(result.columns.ptr[i].len) + CONS_SIZE;
    }

    // Rows: each row is a list of values
    for (size_t i = 0; i < result.rows.len; i++) {
        heap_needed += CONS_SIZE; // row cons cell in outer list
        for (size_t j = 0; j < result.rows.ptr[i].values.len; j++) {
            fermyon_spin_sqlite_value_t *v = &result.rows.ptr[i].values.ptr[j];
            heap_needed += CONS_SIZE;
            switch (v->tag) {
                case FERMYON_SPIN_SQLITE_VALUE_TEXT:
                    heap_needed += term_binary_heap_size(v->val.text.len);
                    break;
                case FERMYON_SPIN_SQLITE_VALUE_BLOB:
                    heap_needed += term_binary_heap_size(v->val.blob.len);
                    break;
                case FERMYON_SPIN_SQLITE_VALUE_REAL:
                    heap_needed += FLOAT_SIZE;
                    break;
                default:
                    heap_needed += 4; // small int or atom
                    break;
            }
        }
    }

    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        fermyon_spin_sqlite_query_result_free(&result);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    // Build columns list
    term columns_list = term_nil();
    for (int i = (int) result.columns.len - 1; i >= 0; i--) {
        term col = term_from_literal_binary(
            result.columns.ptr[i].ptr, result.columns.ptr[i].len, &ctx->heap, glb);
        columns_list = term_list_prepend(col, columns_list, &ctx->heap);
    }

    // Build rows list (list of lists)
    term rows_list = term_nil();
    for (int i = (int) result.rows.len - 1; i >= 0; i--) {
        term row = term_nil();
        for (int j = (int) result.rows.ptr[i].values.len - 1; j >= 0; j--) {
            term val = sqlite_value_to_term(ctx, glb, &result.rows.ptr[i].values.ptr[j]);
            row = term_list_prepend(val, row, &ctx->heap);
        }
        rows_list = term_list_prepend(row, rows_list, &ctx->heap);
    }

    fermyon_spin_sqlite_query_result_free(&result);

    // Build map: #{columns => [...], rows => [...]}
    term columns_key = globalcontext_make_atom(glb, ATOM_STR("\x7", "columns"));
    term rows_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "rows"));

    term result_map = term_alloc_map(2, &ctx->heap);
    term_set_map_assoc(result_map, 0, columns_key, columns_list);
    term_set_map_assoc(result_map, 1, rows_key, rows_list);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, result_map);
    return t;
}

static term nif_spin_sqlite_close(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    if (!term_is_integer(argv[0])) { RAISE_ERROR(BADARG_ATOM); }
    fermyon_spin_sqlite_own_connection_t conn = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_sqlite_connection_drop_own(conn);
    return OK_ATOM;
}

// ===========================================================================
// PostgreSQL NIFs
// ===========================================================================

// Helper: convert Erlang term -> Postgres parameter value
static bool term_to_pg_param(Context *ctx, GlobalContext *glb, term t,
    fermyon_spin_rdbms_types_parameter_value_t *val)
{
    UNUSED(ctx);
    if (term_is_integer(t)) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_INT64;
        val->val.int64 = term_to_int(t);
        return true;
    }
    if (term_is_float(t)) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_FLOATING64;
        val->val.floating64 = term_to_float(t);
        return true;
    }
    if (term_is_binary(t)) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_STR;
        app_string_dup_n(&val->val.str, term_binary_data(t), term_binary_size(t));
        return true;
    }
    if (t == TRUE_ATOM) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_BOOLEAN;
        val->val.boolean = true;
        return true;
    }
    if (t == FALSE_ATOM) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_BOOLEAN;
        val->val.boolean = false;
        return true;
    }
    term null_atom = globalcontext_make_atom(glb, ATOM_STR("\x4", "null"));
    if (t == null_atom || t == UNDEFINED_ATOM) {
        val->tag = FERMYON_SPIN_RDBMS_TYPES_PARAMETER_VALUE_DB_NULL;
        return true;
    }
    return false;
}

// Helper: convert Postgres db-value -> Erlang term
static term pg_value_to_term(Context *ctx, GlobalContext *glb, fermyon_spin_rdbms_types_db_value_t *val)
{
    switch (val->tag) {
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_BOOLEAN:
            return val->val.boolean ? TRUE_ATOM : FALSE_ATOM;
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_INT8:
            return term_from_int(val->val.int8);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_INT16:
            return term_from_int(val->val.int16);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_INT32:
            return term_from_int(val->val.int32);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_INT64:
            return term_from_int64(val->val.int64);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_UINT8:
            return term_from_int(val->val.uint8);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_UINT16:
            return term_from_int(val->val.uint16);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_UINT32:
            return term_from_int(val->val.uint32);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_UINT64:
            return term_from_int64((int64_t) val->val.uint64);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_FLOATING32:
            if (UNLIKELY(memory_ensure_free(ctx, FLOAT_SIZE) != MEMORY_GC_OK)) { return UNDEFINED_ATOM; }
            return term_from_float(val->val.floating32, &ctx->heap);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_FLOATING64:
            if (UNLIKELY(memory_ensure_free(ctx, FLOAT_SIZE) != MEMORY_GC_OK)) { return UNDEFINED_ATOM; }
            return term_from_float(val->val.floating64, &ctx->heap);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_STR:
            return term_from_literal_binary(val->val.str.ptr, val->val.str.len, &ctx->heap, glb);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_BINARY:
            return term_from_literal_binary(val->val.binary.ptr, val->val.binary.len, &ctx->heap, glb);
        case FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_DB_NULL:
            return globalcontext_make_atom(glb, ATOM_STR("\x4", "null"));
        default:
            return UNDEFINED_ATOM;
    }
}

// Helper: build param list from Erlang list
static bool build_pg_params(Context *ctx, GlobalContext *glb, term params_list,
    fermyon_spin_postgres_list_parameter_value_t *out)
{
    out->ptr = NULL;
    out->len = 0;

    if (term_is_nil(params_list) || term_is_invalid_term(params_list)) return true;
    if (!term_is_nonempty_list(params_list)) return true;

    size_t count = 0;
    term cursor = params_list;
    while (term_is_nonempty_list(cursor)) { count++; cursor = term_get_list_tail(cursor); }

    out->ptr = malloc(count * sizeof(fermyon_spin_rdbms_types_parameter_value_t));
    out->len = count;

    cursor = params_list;
    for (size_t i = 0; i < count; i++) {
        term elem = term_get_list_head(cursor);
        if (!term_to_pg_param(ctx, glb, elem, &out->ptr[i])) {
            for (size_t j = 0; j < i; j++) {
                fermyon_spin_rdbms_types_parameter_value_free(&out->ptr[j]);
            }
            free(out->ptr);
            out->ptr = NULL;
            out->len = 0;
            return false;
        }
        cursor = term_get_list_tail(cursor);
    }
    return true;
}

static void free_pg_params(fermyon_spin_postgres_list_parameter_value_t *params)
{
    for (size_t i = 0; i < params->len; i++) {
        fermyon_spin_rdbms_types_parameter_value_free(&params->ptr[i]);
    }
    free(params->ptr);
}

static term nif_spin_postgres_open(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_binary(argv[0])) { RAISE_ERROR(BADARG_ATOM); }

    app_string_t addr;
    app_string_dup_n(&addr, term_binary_data(argv[0]), term_binary_size(argv[0]));

    fermyon_spin_postgres_own_connection_t conn;
    fermyon_spin_postgres_error_t err;
    bool ok = fermyon_spin_postgres_static_connection_open(&addr, &conn, &err);
    app_string_free(&addr);

    if (!ok) {
        fermyon_spin_postgres_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\x11", "connection_failed")));
        return t;
    }

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, term_from_int(conn.__handle));
    return t;
}

static term nif_spin_postgres_query(Context *ctx, int argc, term argv[])
{
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_postgres_own_connection_t conn = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_postgres_borrow_connection_t borrow = fermyon_spin_postgres_borrow_connection(conn);

    app_string_t sql;
    app_string_dup_n(&sql, term_binary_data(argv[1]), term_binary_size(argv[1]));

    fermyon_spin_postgres_list_parameter_value_t params;
    if (!build_pg_params(ctx, glb, argc >= 3 ? argv[2] : term_nil(), &params)) {
        app_string_free(&sql);
        RAISE_ERROR(BADARG_ATOM);
    }

    fermyon_spin_postgres_row_set_t row_set;
    fermyon_spin_postgres_error_t err;
    bool ok = fermyon_spin_postgres_method_connection_query(borrow, &sql, &params, &row_set, &err);
    app_string_free(&sql);
    free_pg_params(&params);

    if (!ok) {
        fermyon_spin_postgres_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xC", "query_failed")));
        return t;
    }

    // Calculate heap needed
    size_t heap_needed = TUPLE_SIZE(2) + TERM_MAP_SIZE(2) + 16;
    for (size_t i = 0; i < row_set.columns.len; i++) {
        heap_needed += term_binary_heap_size(row_set.columns.ptr[i].name.len) + CONS_SIZE;
    }
    for (size_t i = 0; i < row_set.rows.len; i++) {
        heap_needed += CONS_SIZE; // row in outer list
        for (size_t j = 0; j < row_set.rows.ptr[i].len; j++) {
            fermyon_spin_rdbms_types_db_value_t *v = &row_set.rows.ptr[i].ptr[j];
            heap_needed += CONS_SIZE + 8;
            if (v->tag == FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_STR) {
                heap_needed += term_binary_heap_size(v->val.str.len);
            } else if (v->tag == FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_BINARY) {
                heap_needed += term_binary_heap_size(v->val.binary.len);
            } else if (v->tag == FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_FLOATING32
                       || v->tag == FERMYON_SPIN_RDBMS_TYPES_DB_VALUE_FLOATING64) {
                heap_needed += FLOAT_SIZE;
            }
        }
    }

    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        fermyon_spin_postgres_row_set_free(&row_set);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    // Build columns list (column names)
    term columns_list = term_nil();
    for (int i = (int) row_set.columns.len - 1; i >= 0; i--) {
        term col = term_from_literal_binary(
            row_set.columns.ptr[i].name.ptr, row_set.columns.ptr[i].name.len, &ctx->heap, glb);
        columns_list = term_list_prepend(col, columns_list, &ctx->heap);
    }

    // Build rows (list of lists)
    term rows_list = term_nil();
    for (int i = (int) row_set.rows.len - 1; i >= 0; i--) {
        term row = term_nil();
        for (int j = (int) row_set.rows.ptr[i].len - 1; j >= 0; j--) {
            term val = pg_value_to_term(ctx, glb, &row_set.rows.ptr[i].ptr[j]);
            row = term_list_prepend(val, row, &ctx->heap);
        }
        rows_list = term_list_prepend(row, rows_list, &ctx->heap);
    }

    fermyon_spin_postgres_row_set_free(&row_set);

    term columns_key = globalcontext_make_atom(glb, ATOM_STR("\x7", "columns"));
    term rows_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "rows"));

    term result_map = term_alloc_map(2, &ctx->heap);
    term_set_map_assoc(result_map, 0, columns_key, columns_list);
    term_set_map_assoc(result_map, 1, rows_key, rows_list);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, result_map);
    return t;
}

static term nif_spin_postgres_execute(Context *ctx, int argc, term argv[])
{
    GlobalContext *glb = ctx->global;

    if (!term_is_integer(argv[0]) || !term_is_binary(argv[1])) { RAISE_ERROR(BADARG_ATOM); }

    fermyon_spin_postgres_own_connection_t conn = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_postgres_borrow_connection_t borrow = fermyon_spin_postgres_borrow_connection(conn);

    app_string_t sql;
    app_string_dup_n(&sql, term_binary_data(argv[1]), term_binary_size(argv[1]));

    fermyon_spin_postgres_list_parameter_value_t params;
    if (!build_pg_params(ctx, glb, argc >= 3 ? argv[2] : term_nil(), &params)) {
        app_string_free(&sql);
        RAISE_ERROR(BADARG_ATOM);
    }

    uint64_t rows_affected;
    fermyon_spin_postgres_error_t err;
    bool ok = fermyon_spin_postgres_method_connection_execute(borrow, &sql, &params, &rows_affected, &err);
    app_string_free(&sql);
    free_pg_params(&params);

    if (!ok) {
        fermyon_spin_postgres_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xE", "execute_failed")));
        return t;
    }

    if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, term_from_int((avm_int_t) rows_affected));
    return t;
}

static term nif_spin_postgres_close(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    if (!term_is_integer(argv[0])) { RAISE_ERROR(BADARG_ATOM); }
    fermyon_spin_postgres_own_connection_t conn = { .__handle = term_to_int(argv[0]) };
    fermyon_spin_postgres_connection_drop_own(conn);
    return OK_ATOM;
}

// ===========================================================================
// Config Store NIFs (wasi:config/store)
// ===========================================================================

static term nif_spin_config_get(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;

    if (!term_is_binary(argv[0])) { RAISE_ERROR(BADARG_ATOM); }

    app_string_t key;
    app_string_dup_n(&key, term_binary_data(argv[0]), term_binary_size(argv[0]));

    app_option_string_t val;
    wasi_config_store_error_t err;
    bool ok = wasi_config_store_get(&key, &val, &err);
    app_string_free(&key);

    if (!ok) {
        wasi_config_store_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xC", "config_error")));
        return t;
    }

    if (!val.is_some) {
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, OK_ATOM);
        term_put_tuple_element(t, 1, UNDEFINED_ATOM);
        return t;
    }

    size_t heap_needed = TUPLE_SIZE(2) + term_binary_heap_size(val.val.len);
    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        app_string_free(&val.val);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term bin = term_from_literal_binary(val.val.ptr, val.val.len, &ctx->heap, glb);
    app_string_free(&val.val);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, bin);
    return t;
}

static term nif_spin_config_get_all(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    UNUSED(argv);
    GlobalContext *glb = ctx->global;

    app_list_tuple2_string_string_t entries;
    wasi_config_store_error_t err;
    bool ok = wasi_config_store_get_all(&entries, &err);

    if (!ok) {
        wasi_config_store_error_free(&err);
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) { RAISE_ERROR(OUT_OF_MEMORY_ATOM); }
        term t = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(t, 0, ERROR_ATOM);
        term_put_tuple_element(t, 1, globalcontext_make_atom(glb, ATOM_STR("\xC", "config_error")));
        return t;
    }

    // Calculate heap: list of {Key, Value} tuples
    size_t heap_needed = TUPLE_SIZE(2);
    for (size_t i = 0; i < entries.len; i++) {
        heap_needed += TUPLE_SIZE(2) + CONS_SIZE
            + term_binary_heap_size(entries.ptr[i].f0.len)
            + term_binary_heap_size(entries.ptr[i].f1.len);
    }
    if (UNLIKELY(memory_ensure_free(ctx, heap_needed) != MEMORY_GC_OK)) {
        app_list_tuple2_string_string_free(&entries);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    term list = term_nil();
    for (int i = (int) entries.len - 1; i >= 0; i--) {
        term k = term_from_literal_binary(entries.ptr[i].f0.ptr, entries.ptr[i].f0.len, &ctx->heap, glb);
        term v = term_from_literal_binary(entries.ptr[i].f1.ptr, entries.ptr[i].f1.len, &ctx->heap, glb);
        term tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(tuple, 0, k);
        term_put_tuple_element(tuple, 1, v);
        list = term_list_prepend(tuple, list, &ctx->heap);
    }
    app_list_tuple2_string_string_free(&entries);

    term t = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(t, 0, OK_ATOM);
    term_put_tuple_element(t, 1, list);
    return t;
}

// ===========================================================================
// NIF Registration
// ===========================================================================

#define MAKE_NIF(id, func) \
    static const struct Nif id = { .base.type = NIFFunctionType, .nif_ptr = func };

MAKE_NIF(nif_kv_open, nif_spin_kv_open)
MAKE_NIF(nif_kv_get, nif_spin_kv_get)
MAKE_NIF(nif_kv_set, nif_spin_kv_set)
MAKE_NIF(nif_kv_delete, nif_spin_kv_delete)
MAKE_NIF(nif_kv_exists, nif_spin_kv_exists)
MAKE_NIF(nif_kv_get_keys, nif_spin_kv_get_keys)
MAKE_NIF(nif_kv_close, nif_spin_kv_close)
MAKE_NIF(nif_sqlite_open, nif_spin_sqlite_open)
MAKE_NIF(nif_sqlite_execute, nif_spin_sqlite_execute)
MAKE_NIF(nif_sqlite_close, nif_spin_sqlite_close)
MAKE_NIF(nif_pg_open, nif_spin_postgres_open)
MAKE_NIF(nif_pg_query, nif_spin_postgres_query)
MAKE_NIF(nif_pg_execute, nif_spin_postgres_execute)
MAKE_NIF(nif_pg_close, nif_spin_postgres_close)
MAKE_NIF(nif_config_get, nif_spin_config_get)
MAKE_NIF(nif_config_get_all, nif_spin_config_get_all)

const struct Nif *wasi_spin_nifs_get_nif(const char *nifname)
{
    // KV
    if (strcmp(nifname, "spin_kv:open/1") == 0) return &nif_kv_open;
    if (strcmp(nifname, "spin_kv:get/2") == 0) return &nif_kv_get;
    if (strcmp(nifname, "spin_kv:set/3") == 0) return &nif_kv_set;
    if (strcmp(nifname, "spin_kv:delete/2") == 0) return &nif_kv_delete;
    if (strcmp(nifname, "spin_kv:exists/2") == 0) return &nif_kv_exists;
    if (strcmp(nifname, "spin_kv:get_keys/1") == 0) return &nif_kv_get_keys;
    if (strcmp(nifname, "spin_kv:close/1") == 0) return &nif_kv_close;
    // SQLite
    if (strcmp(nifname, "spin_sqlite:open/1") == 0) return &nif_sqlite_open;
    if (strcmp(nifname, "spin_sqlite:execute/2") == 0) return &nif_sqlite_execute;
    if (strcmp(nifname, "spin_sqlite:execute/3") == 0) return &nif_sqlite_execute;
    if (strcmp(nifname, "spin_sqlite:close/1") == 0) return &nif_sqlite_close;
    // Postgres
    if (strcmp(nifname, "spin_postgres:open/1") == 0) return &nif_pg_open;
    if (strcmp(nifname, "spin_postgres:query/2") == 0) return &nif_pg_query;
    if (strcmp(nifname, "spin_postgres:query/3") == 0) return &nif_pg_query;
    if (strcmp(nifname, "spin_postgres:execute/2") == 0) return &nif_pg_execute;
    if (strcmp(nifname, "spin_postgres:execute/3") == 0) return &nif_pg_execute;
    if (strcmp(nifname, "spin_postgres:close/1") == 0) return &nif_pg_close;
    // Config
    if (strcmp(nifname, "spin_config:get/1") == 0) return &nif_config_get;
    if (strcmp(nifname, "spin_config:get_all/0") == 0) return &nif_config_get_all;

    return NULL;
}
