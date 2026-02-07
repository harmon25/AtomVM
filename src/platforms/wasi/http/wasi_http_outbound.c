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
 * @file wasi_http_outbound.c
 * @brief NIF implementation for outbound HTTP requests via wasi:http/outgoing-handler.
 *
 * Exposes spin_http:request/1 to Erlang/Elixir code:
 *
 *   spin_http:request(#{
 *       method  => get | post | put | delete | head | options | patch,
 *       url     => <<"https://example.com/api">>,
 *       headers => [{<<"content-type">>, <<"application/json">>}],
 *       body    => <<"...">>
 *   }).
 *   %% => {ok, #{status => 200, headers => [...], body => <<...>>}}
 *   %% => {error, Reason}
 */

#include "wasi_http_outbound.h"
#include "generated/app.h"

#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "memory.h"
#include "term.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Helpers: parse URL into scheme, authority, path
// ---------------------------------------------------------------------------

typedef struct {
    const char *scheme;       // "http" or "https"
    size_t scheme_len;
    const char *authority;    // "host:port"
    size_t authority_len;
    const char *path;         // "/path?query"
    size_t path_len;
} parsed_url_t;

static bool parse_url(const char *url, size_t url_len, parsed_url_t *out)
{
    memset(out, 0, sizeof(*out));

    // Find scheme
    const char *scheme_end = NULL;
    for (size_t i = 0; i + 2 < url_len; i++) {
        if (url[i] == ':' && url[i + 1] == '/' && url[i + 2] == '/') {
            scheme_end = url + i;
            break;
        }
    }

    if (!scheme_end) {
        return false;
    }

    out->scheme = url;
    out->scheme_len = (size_t)(scheme_end - url);

    // Authority starts after "://"
    const char *authority_start = scheme_end + 3;
    size_t remaining = url_len - (size_t)(authority_start - url);

    // Find end of authority (first '/' after ://)
    const char *path_start = NULL;
    for (size_t i = 0; i < remaining; i++) {
        if (authority_start[i] == '/') {
            path_start = authority_start + i;
            break;
        }
    }

    if (path_start) {
        out->authority = authority_start;
        out->authority_len = (size_t)(path_start - authority_start);
        out->path = path_start;
        out->path_len = url_len - (size_t)(path_start - url);
    } else {
        // No path — authority is the rest
        out->authority = authority_start;
        out->authority_len = remaining;
        out->path = "/";
        out->path_len = 1;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Helpers: Erlang atom -> WASI method
// ---------------------------------------------------------------------------

static bool atom_to_method(GlobalContext *glb, term atom, wasi_http_types_method_t *method)
{
    AtomString get_str = ATOM_STR("\x3", "get");
    AtomString post_str = ATOM_STR("\x4", "post");
    AtomString put_str = ATOM_STR("\x3", "put");
    AtomString delete_str = ATOM_STR("\x6", "delete");
    AtomString head_str = ATOM_STR("\x4", "head");
    AtomString options_str = ATOM_STR("\x7", "options");
    AtomString patch_str = ATOM_STR("\x5", "patch");

    term get_atom = globalcontext_make_atom(glb, get_str);
    term post_atom = globalcontext_make_atom(glb, post_str);
    term put_atom = globalcontext_make_atom(glb, put_str);
    term delete_atom = globalcontext_make_atom(glb, delete_str);
    term head_atom = globalcontext_make_atom(glb, head_str);
    term options_atom = globalcontext_make_atom(glb, options_str);
    term patch_atom = globalcontext_make_atom(glb, patch_str);

    if (atom == get_atom) { method->tag = WASI_HTTP_TYPES_METHOD_GET; return true; }
    if (atom == post_atom) { method->tag = WASI_HTTP_TYPES_METHOD_POST; return true; }
    if (atom == put_atom) { method->tag = WASI_HTTP_TYPES_METHOD_PUT; return true; }
    if (atom == delete_atom) { method->tag = WASI_HTTP_TYPES_METHOD_DELETE; return true; }
    if (atom == head_atom) { method->tag = WASI_HTTP_TYPES_METHOD_HEAD; return true; }
    if (atom == options_atom) { method->tag = WASI_HTTP_TYPES_METHOD_OPTIONS; return true; }
    if (atom == patch_atom) { method->tag = WASI_HTTP_TYPES_METHOD_PATCH; return true; }

    return false;
}

// ---------------------------------------------------------------------------
// Helper: read an entire incoming response body
// ---------------------------------------------------------------------------

static void read_response_body(
    wasi_http_types_own_incoming_response_t response,
    uint8_t **out_data, size_t *out_len)
{
    *out_data = NULL;
    *out_len = 0;

    wasi_http_types_borrow_incoming_response_t resp_borrow
        = wasi_http_types_borrow_incoming_response(response);
    wasi_http_types_own_incoming_body_t body_handle;
    if (!wasi_http_types_method_incoming_response_consume(resp_borrow, &body_handle)) {
        return;
    }

    wasi_http_types_borrow_incoming_body_t body_borrow
        = wasi_http_types_borrow_incoming_body(body_handle);
    wasi_http_types_own_input_stream_t stream_handle;
    if (!wasi_http_types_method_incoming_body_stream(body_borrow, &stream_handle)) {
        wasi_http_types_incoming_body_drop_own(body_handle);
        return;
    }

    size_t capacity = 4096;
    size_t total = 0;
    uint8_t *buf = malloc(capacity);
    if (!buf) {
        wasi_io_streams_input_stream_drop_own(stream_handle);
        wasi_http_types_incoming_body_drop_own(body_handle);
        return;
    }

    wasi_io_streams_borrow_input_stream_t stream_borrow
        = wasi_io_streams_borrow_input_stream(stream_handle);

    for (;;) {
        app_list_u8_t chunk;
        wasi_io_streams_stream_error_t err;
        bool ok = wasi_io_streams_method_input_stream_blocking_read(
            stream_borrow, 65536, &chunk, &err);
        if (!ok) break;
        if (chunk.len == 0) { app_list_u8_free(&chunk); break; }

        while (total + chunk.len > capacity) {
            capacity *= 2;
            uint8_t *new_buf = realloc(buf, capacity);
            if (!new_buf) {
                free(buf);
                app_list_u8_free(&chunk);
                wasi_io_streams_input_stream_drop_own(stream_handle);
                wasi_http_types_incoming_body_drop_own(body_handle);
                *out_data = NULL;
                *out_len = 0;
                return;
            }
            buf = new_buf;
        }
        memcpy(buf + total, chunk.ptr, chunk.len);
        total += chunk.len;
        app_list_u8_free(&chunk);
    }

    wasi_io_streams_input_stream_drop_own(stream_handle);
    wasi_http_types_own_future_trailers_t trailers
        = wasi_http_types_static_incoming_body_finish(body_handle);
    wasi_http_types_future_trailers_drop_own(trailers);

    *out_data = buf;
    *out_len = total;
}

// ---------------------------------------------------------------------------
// NIF: spin_http:request/1
// ---------------------------------------------------------------------------

static term nif_spin_http_request(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    GlobalContext *glb = ctx->global;
    term req_map = argv[0];

    if (!term_is_map(req_map)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    // -- Extract fields from the request map --

    // url (required)
    term url_key = globalcontext_make_atom(glb, ATOM_STR("\x3", "url"));
    term url_val = term_get_map_assoc(req_map, url_key, glb);
    if (term_is_invalid_term(url_val) || !term_is_binary(url_val)) {
        RAISE_ERROR(BADARG_ATOM);
    }
    const char *url_ptr = term_binary_data(url_val);
    size_t url_len = term_binary_size(url_val);

    // method (default: get)
    wasi_http_types_method_t method = { .tag = WASI_HTTP_TYPES_METHOD_GET };
    term method_key = globalcontext_make_atom(glb, ATOM_STR("\x6", "method"));
    term method_val = term_get_map_assoc(req_map, method_key, glb);
    if (!term_is_invalid_term(method_val) && term_is_atom(method_val)) {
        if (!atom_to_method(glb, method_val, &method)) {
            RAISE_ERROR(BADARG_ATOM);
        }
    }

    // headers (default: [])
    term headers_key = globalcontext_make_atom(glb, ATOM_STR("\x7", "headers"));
    term headers_val = term_get_map_assoc(req_map, headers_key, glb);

    // body (default: <<>>)
    term body_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "body"));
    term body_val = term_get_map_assoc(req_map, body_key, glb);

    // -- Parse URL --
    parsed_url_t parsed;
    if (!parse_url(url_ptr, url_len, &parsed)) {
        RAISE_ERROR(BADARG_ATOM);
    }

    // -- Build WASI outgoing request --

    // Headers
    wasi_http_types_own_fields_t wasi_headers = wasi_http_types_constructor_fields();
    wasi_http_types_borrow_fields_t wasi_headers_borrow
        = wasi_http_types_borrow_fields(wasi_headers);

    if (!term_is_invalid_term(headers_val) && term_is_nonempty_list(headers_val)) {
        term cursor = headers_val;
        while (term_is_nonempty_list(cursor)) {
            term entry = term_get_list_head(cursor);
            if (term_is_tuple(entry) && term_get_tuple_arity(entry) == 2) {
                term hdr_name = term_get_tuple_element(entry, 0);
                term hdr_value = term_get_tuple_element(entry, 1);
                if (term_is_binary(hdr_name) && term_is_binary(hdr_value)) {
                    wasi_http_types_field_name_t wn = {
                        .ptr = (uint8_t *) term_binary_data(hdr_name),
                        .len = term_binary_size(hdr_name)
                    };
                    wasi_http_types_field_value_t wv = {
                        .ptr = (uint8_t *) term_binary_data(hdr_value),
                        .len = term_binary_size(hdr_value)
                    };
                    wasi_http_types_header_error_t hdr_err;
                    wasi_http_types_method_fields_append(
                        wasi_headers_borrow, &wn, &wv, &hdr_err);
                }
            }
            cursor = term_get_list_tail(cursor);
        }
    }

    // Create the outgoing request
    wasi_http_types_own_outgoing_request_t wasi_req
        = wasi_http_types_constructor_outgoing_request(wasi_headers);
    wasi_http_types_borrow_outgoing_request_t req_borrow
        = wasi_http_types_borrow_outgoing_request(wasi_req);

    // Set method
    wasi_http_types_method_outgoing_request_set_method(req_borrow, &method);

    // Set scheme
    wasi_http_types_scheme_t scheme;
    if (parsed.scheme_len == 5 && memcmp(parsed.scheme, "https", 5) == 0) {
        scheme.tag = WASI_HTTP_TYPES_SCHEME_HTTPS;
    } else {
        scheme.tag = WASI_HTTP_TYPES_SCHEME_HTTP;
    }
    wasi_http_types_method_outgoing_request_set_scheme(req_borrow, &scheme);

    // Set authority
    app_string_t authority_str;
    app_string_dup_n(&authority_str, parsed.authority, parsed.authority_len);
    wasi_http_types_method_outgoing_request_set_authority(req_borrow, &authority_str);
    app_string_free(&authority_str);

    // Set path
    app_string_t path_str;
    app_string_dup_n(&path_str, parsed.path, parsed.path_len);
    wasi_http_types_method_outgoing_request_set_path_with_query(req_borrow, &path_str);
    app_string_free(&path_str);

    // Write body if present
    if (!term_is_invalid_term(body_val) && term_is_binary(body_val)
        && term_binary_size(body_val) > 0) {
        wasi_http_types_own_outgoing_body_t req_body;
        if (wasi_http_types_method_outgoing_request_body(req_borrow, &req_body)) {
            wasi_http_types_borrow_outgoing_body_t body_borrow
                = wasi_http_types_borrow_outgoing_body(req_body);
            wasi_http_types_own_output_stream_t out_stream;
            if (wasi_http_types_method_outgoing_body_write(body_borrow, &out_stream)) {
                wasi_io_streams_borrow_output_stream_t stream_borrow
                    = wasi_io_streams_borrow_output_stream(out_stream);
                app_list_u8_t body_bytes = {
                    .ptr = (uint8_t *) term_binary_data(body_val),
                    .len = term_binary_size(body_val)
                };
                wasi_io_streams_stream_error_t stream_err;
                wasi_io_streams_method_output_stream_blocking_write_and_flush(
                    stream_borrow, &body_bytes, &stream_err);
                wasi_io_streams_output_stream_drop_own(out_stream);
            }
            wasi_http_types_error_code_t finish_err;
            wasi_http_types_static_outgoing_body_finish(req_body, NULL, &finish_err);
        }
    }

    // -- Send the request via outgoing-handler --

    wasi_http_outgoing_handler_own_future_incoming_response_t future;
    wasi_http_outgoing_handler_error_code_t send_err;
    bool sent = wasi_http_outgoing_handler_handle(wasi_req, NULL, &future, &send_err);

    if (!sent) {
        // Return {error, request_failed}
        wasi_http_outgoing_handler_error_code_free(&send_err);
        term error_atom = globalcontext_make_atom(glb, ATOM_STR("\xE", "request_failed"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, error_atom);
        return error_tuple;
    }

    // -- Block waiting for the response --

    // Subscribe to the future and block
    wasi_http_types_borrow_future_incoming_response_t future_borrow
        = wasi_http_types_borrow_future_incoming_response(future);
    wasi_http_types_own_pollable_t pollable
        = wasi_http_types_method_future_incoming_response_subscribe(future_borrow);
    wasi_io_poll_borrow_pollable_t poll_borrow
        = wasi_io_poll_borrow_pollable(pollable);
    wasi_io_poll_method_pollable_block(poll_borrow);
    wasi_io_poll_pollable_drop_own(pollable);

    // Get the response
    wasi_http_types_result_result_own_incoming_response_error_code_void_t resp_result;
    bool got = wasi_http_types_method_future_incoming_response_get(future_borrow, &resp_result);
    wasi_http_types_future_incoming_response_drop_own(future);

    if (!got || resp_result.is_err) {
        term error_atom = globalcontext_make_atom(glb, ATOM_STR("\x10", "response_timeout"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, error_atom);
        return error_tuple;
    }

    // The ok value is itself a result<incoming-response, error-code>
    if (resp_result.val.ok.is_err) {
        wasi_http_types_error_code_free(&resp_result.val.ok.val.err);
        term error_atom = globalcontext_make_atom(glb, ATOM_STR("\xA", "http_error"));
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term error_tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error_tuple, 0, ERROR_ATOM);
        term_put_tuple_element(error_tuple, 1, error_atom);
        return error_tuple;
    }

    wasi_http_types_own_incoming_response_t response = resp_result.val.ok.val.ok;

    // -- Read response status, headers, body --

    wasi_http_types_borrow_incoming_response_t resp_borrow
        = wasi_http_types_borrow_incoming_response(response);

    uint16_t status = wasi_http_types_method_incoming_response_status(resp_borrow);

    // Headers
    wasi_http_types_own_headers_t resp_headers_handle
        = wasi_http_types_method_incoming_response_headers(resp_borrow);
    wasi_http_types_borrow_fields_t resp_headers_borrow
        = wasi_http_types_borrow_fields(resp_headers_handle);
    app_list_tuple2_field_name_field_value_t resp_header_entries;
    wasi_http_types_method_fields_entries(resp_headers_borrow, &resp_header_entries);

    // Body
    uint8_t *resp_body_data = NULL;
    size_t resp_body_len = 0;
    read_response_body(response, &resp_body_data, &resp_body_len);

    // -- Build Erlang response: {ok, #{status => N, headers => [...], body => <<...>>}} --

    size_t body_heap = term_binary_heap_size(resp_body_len);
    size_t hdrs_heap = 0;
    for (size_t i = 0; i < resp_header_entries.len; i++) {
        hdrs_heap += TUPLE_SIZE(2)
            + term_binary_heap_size(resp_header_entries.ptr[i].f0.len)
            + term_binary_heap_size(resp_header_entries.ptr[i].f1.len)
            + CONS_SIZE;
    }

    size_t total_heap = TUPLE_SIZE(2) + TERM_MAP_SIZE(3) + body_heap + hdrs_heap + 16;
    if (UNLIKELY(memory_ensure_free(ctx, total_heap) != MEMORY_GC_OK)) {
        free(resp_body_data);
        wasi_http_types_fields_drop_own(resp_headers_handle);
        app_list_tuple2_field_name_field_value_free(&resp_header_entries);
        wasi_http_types_incoming_response_drop_own(response);
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    // Build headers list
    term hdrs_list = term_nil();
    for (int i = (int) resp_header_entries.len - 1; i >= 0; i--) {
        term name_bin = term_from_literal_binary(
            (const void *) resp_header_entries.ptr[i].f0.ptr,
            resp_header_entries.ptr[i].f0.len, &ctx->heap, glb);
        term value_bin = term_from_literal_binary(
            (const void *) resp_header_entries.ptr[i].f1.ptr,
            resp_header_entries.ptr[i].f1.len, &ctx->heap, glb);
        term tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(tuple, 0, name_bin);
        term_put_tuple_element(tuple, 1, value_bin);
        hdrs_list = term_list_prepend(tuple, hdrs_list, &ctx->heap);
    }

    term body_bin = term_from_literal_binary(
        (const void *) resp_body_data, resp_body_len, &ctx->heap, glb);

    // Build the map
    term status_key = globalcontext_make_atom(glb, ATOM_STR("\x6", "status"));
    term resp_headers_key = globalcontext_make_atom(glb, ATOM_STR("\x7", "headers"));
    term resp_body_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "body"));

    term resp_map = term_alloc_map(3, &ctx->heap);
    term_set_map_assoc(resp_map, 0, resp_body_key, body_bin);
    term_set_map_assoc(resp_map, 1, resp_headers_key, hdrs_list);
    term_set_map_assoc(resp_map, 2, status_key, term_from_int(status));

    // Wrap in {ok, Map}
    term ok_tuple = term_alloc_tuple(2, &ctx->heap);
    term_put_tuple_element(ok_tuple, 0, OK_ATOM);
    term_put_tuple_element(ok_tuple, 1, resp_map);

    // Cleanup
    free(resp_body_data);
    wasi_http_types_fields_drop_own(resp_headers_handle);
    app_list_tuple2_field_name_field_value_free(&resp_header_entries);
    wasi_http_types_incoming_response_drop_own(response);

    return ok_tuple;
}

// ---------------------------------------------------------------------------
// NIF registration
// ---------------------------------------------------------------------------

static const struct Nif spin_http_request_nif = {
    .base.type = NIFFunctionType,
    .nif_ptr = nif_spin_http_request
};

const struct Nif *wasi_http_outbound_get_nif(const char *nifname)
{
    if (strcmp(nifname, "spin_http:request/1") == 0) {
        return &spin_http_request_nif;
    }
    return NULL;
}
