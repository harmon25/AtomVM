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
 * @file wasi_http_handler.c
 * @brief Implementation of the wasi:http/incoming-handler export for AtomVM.
 *
 * This file implements the `handle` function required by the
 * `wasi:http/incoming-handler` interface. When Spin (or wasmtime serve)
 * receives an HTTP request, it calls this exported function. We:
 *
 * 1. Extract the HTTP request data (method, path, headers, body) from
 *    WASI component model resources.
 * 2. Build an Erlang map representing the request.
 * 3. Call the user's Erlang handler function (spin_handler:handle/1).
 * 4. Parse the Erlang response map and send it back via WASI's
 *    response-outparam.
 */

#include "generated/app.h"
#include "wasi_http_handler.h"

#include "atom.h"
#include "context.h"
#include "defaultatoms.h"
#include "globalcontext.h"
#include "memory.h"
#include "module.h"
#include "platform_defaultatoms.h"
#include "term.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Global state: the AtomVM instance persists across requests (reactor mode)
// ---------------------------------------------------------------------------

static GlobalContext *g_glb = NULL;

void wasi_http_handler_init(GlobalContext *glb)
{
    g_glb = glb;
}

GlobalContext *wasi_http_handler_get_global(void)
{
    return g_glb;
}

// ---------------------------------------------------------------------------
// Helper: read the entire incoming body as a byte buffer
// ---------------------------------------------------------------------------

static void read_incoming_body(
    wasi_http_types_own_incoming_request_t request_handle,
    uint8_t **out_data, size_t *out_len)
{
    *out_data = NULL;
    *out_len = 0;

    // Consume the body from the request
    wasi_http_types_borrow_incoming_request_t req_borrow
        = wasi_http_types_borrow_incoming_request(request_handle);
    wasi_http_types_own_incoming_body_t body_handle;
    if (!wasi_http_types_method_incoming_request_consume(req_borrow, &body_handle)) {
        return; // No body or already consumed
    }

    // Get the input stream from the body
    wasi_http_types_borrow_incoming_body_t body_borrow
        = wasi_http_types_borrow_incoming_body(body_handle);
    wasi_http_types_own_input_stream_t stream_handle;
    if (!wasi_http_types_method_incoming_body_stream(body_borrow, &stream_handle)) {
        wasi_http_types_incoming_body_drop_own(body_handle);
        return;
    }

    // Read all available data in a loop
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
        if (!ok) {
            // Stream closed or error — we're done reading
            break;
        }
        if (chunk.len == 0) {
            app_list_u8_free(&chunk);
            break;
        }
        // Grow buffer if needed
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
    // Finish the body (required by the spec — consumes body, returns future-trailers)
    wasi_http_types_own_future_trailers_t trailers = wasi_http_types_static_incoming_body_finish(body_handle);
    wasi_http_types_future_trailers_drop_own(trailers);

    *out_data = buf;
    *out_len = total;
}

// ---------------------------------------------------------------------------
// Helper: convert HTTP method to an Erlang atom
// ---------------------------------------------------------------------------

static term method_to_atom(GlobalContext *glb, wasi_http_types_method_t *method)
{
    switch (method->tag) {
        case WASI_HTTP_TYPES_METHOD_GET:
            return globalcontext_make_atom(glb, ATOM_STR("\x3", "get"));
        case WASI_HTTP_TYPES_METHOD_HEAD:
            return globalcontext_make_atom(glb, ATOM_STR("\x4", "head"));
        case WASI_HTTP_TYPES_METHOD_POST:
            return globalcontext_make_atom(glb, ATOM_STR("\x4", "post"));
        case WASI_HTTP_TYPES_METHOD_PUT:
            return globalcontext_make_atom(glb, ATOM_STR("\x3", "put"));
        case WASI_HTTP_TYPES_METHOD_DELETE:
            return globalcontext_make_atom(glb, ATOM_STR("\x6", "delete"));
        case WASI_HTTP_TYPES_METHOD_CONNECT:
            return globalcontext_make_atom(glb, ATOM_STR("\x7", "connect"));
        case WASI_HTTP_TYPES_METHOD_OPTIONS:
            return globalcontext_make_atom(glb, ATOM_STR("\x7", "options"));
        case WASI_HTTP_TYPES_METHOD_TRACE:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "trace"));
        case WASI_HTTP_TYPES_METHOD_PATCH:
            return globalcontext_make_atom(glb, ATOM_STR("\x5", "patch"));
        case WASI_HTTP_TYPES_METHOD_OTHER:
        default: {
            // Create an atom from the custom method string
            // Build length-prefixed atom string
            size_t len = method->val.other.len;
            if (len > 255) len = 255;
            char atom_buf[257];
            atom_buf[0] = (char) len;
            memcpy(atom_buf + 1, method->val.other.ptr, len);
            return globalcontext_make_atom(glb, atom_buf);
        }
    }
}

// ---------------------------------------------------------------------------
// Helper: send an error response when something goes wrong
// ---------------------------------------------------------------------------

static void send_error_response(
    wasi_http_types_own_response_outparam_t response_out,
    uint16_t status_code,
    const char *body_text)
{
    // Create response headers
    wasi_http_types_own_fields_t resp_headers = wasi_http_types_constructor_fields();
    wasi_http_types_borrow_fields_t resp_headers_borrow
        = wasi_http_types_borrow_fields(resp_headers);

    // Set content-type to text/plain
    wasi_http_types_field_name_t ct_name;
    app_string_set(&ct_name, "content-type");
    wasi_http_types_field_value_t ct_value = {
        .ptr = (uint8_t *) "text/plain",
        .len = 10
    };
    wasi_http_types_header_error_t header_err;
    wasi_http_types_method_fields_append(resp_headers_borrow, &ct_name, &ct_value, &header_err);

    // Create outgoing response
    wasi_http_types_own_outgoing_response_t response
        = wasi_http_types_constructor_outgoing_response(resp_headers);
    wasi_http_types_borrow_outgoing_response_t resp_borrow
        = wasi_http_types_borrow_outgoing_response(response);
    wasi_http_types_method_outgoing_response_set_status_code(resp_borrow, status_code);

    // Write body
    wasi_http_types_own_outgoing_body_t resp_body;
    if (wasi_http_types_method_outgoing_response_body(resp_borrow, &resp_body)) {
        wasi_http_types_borrow_outgoing_body_t body_borrow
            = wasi_http_types_borrow_outgoing_body(resp_body);
        wasi_http_types_own_output_stream_t out_stream;
        if (wasi_http_types_method_outgoing_body_write(body_borrow, &out_stream)) {
            wasi_io_streams_borrow_output_stream_t stream_borrow
                = wasi_io_streams_borrow_output_stream(out_stream);
            app_list_u8_t body_data = {
                .ptr = (uint8_t *) body_text,
                .len = strlen(body_text)
            };
            wasi_io_streams_stream_error_t stream_err;
            wasi_io_streams_method_output_stream_blocking_write_and_flush(
                stream_borrow, &body_data, &stream_err);
            wasi_io_streams_output_stream_drop_own(out_stream);
        }
        wasi_http_types_error_code_t finish_err;
        wasi_http_types_own_trailers_t *no_trailers = NULL;
        wasi_http_types_static_outgoing_body_finish(resp_body, no_trailers, &finish_err);
    }

    // Set the response
    wasi_http_types_result_own_outgoing_response_error_code_t result;
    result.is_err = false;
    result.val.ok = response;
    wasi_http_types_static_response_outparam_set(response_out, &result);
}

// ---------------------------------------------------------------------------
// The exported wasi:http/incoming-handler.handle function
// ---------------------------------------------------------------------------

void exports_wasi_http_incoming_handler_handle(
    exports_wasi_http_incoming_handler_own_incoming_request_t request,
    exports_wasi_http_incoming_handler_own_response_outparam_t response_out)
{
    if (!g_glb) {
        send_error_response(response_out, 500, "AtomVM not initialized");
        wasi_http_types_incoming_request_drop_own(request);
        return;
    }

    GlobalContext *glb = g_glb;

    // -----------------------------------------------------------------------
    // 1. Extract request data from WASI component model resources
    // -----------------------------------------------------------------------

    wasi_http_types_borrow_incoming_request_t req_borrow
        = wasi_http_types_borrow_incoming_request(request);

    // Method
    wasi_http_types_method_t method;
    wasi_http_types_method_incoming_request_method(req_borrow, &method);

    // Path with query
    app_string_t path_str = { .ptr = NULL, .len = 0 };
    bool has_path = wasi_http_types_method_incoming_request_path_with_query(req_borrow, &path_str);

    // Authority (host)
    app_string_t authority_str = { .ptr = NULL, .len = 0 };
    bool has_authority = wasi_http_types_method_incoming_request_authority(req_borrow, &authority_str);

    // Headers
    wasi_http_types_own_headers_t headers_handle
        = wasi_http_types_method_incoming_request_headers(req_borrow);
    wasi_http_types_borrow_fields_t headers_borrow
        = wasi_http_types_borrow_fields(headers_handle);
    app_list_tuple2_field_name_field_value_t header_entries;
    wasi_http_types_method_fields_entries(headers_borrow, &header_entries);

    // Body
    uint8_t *body_data = NULL;
    size_t body_len = 0;
    read_incoming_body(request, &body_data, &body_len);

    // -----------------------------------------------------------------------
    // 2. Build an Erlang map representing the request:
    //    #{method => atom, path => binary, headers => [{binary,binary}], body => binary}
    // -----------------------------------------------------------------------

    // Calculate heap needed:
    // - Map with 4-5 entries: TERM_MAP_SIZE(5) = 3 + 2*5 = 13
    // - method atom: 0 (atoms are inline)
    // - path binary
    // - authority binary (optional)
    // - headers list: each entry is a 2-tuple + 2 binaries + cons cell
    // - body binary
    size_t path_heap = has_path ? term_binary_heap_size(path_str.len) : 0;
    size_t authority_heap = has_authority ? term_binary_heap_size(authority_str.len) : 0;
    size_t body_heap = term_binary_heap_size(body_len);

    // Headers: each entry needs TUPLE_SIZE(2) + 2 * binary_heap_size + CONS_SIZE
    size_t headers_heap = 0;
    for (size_t i = 0; i < header_entries.len; i++) {
        headers_heap += TUPLE_SIZE(2)
            + term_binary_heap_size(header_entries.ptr[i].f0.len)
            + term_binary_heap_size(header_entries.ptr[i].f1.len)
            + CONS_SIZE;
    }

    int num_map_keys = 4; // method, path, headers, body
    if (has_authority) num_map_keys = 5;

    size_t total_heap = TERM_MAP_SIZE(num_map_keys)
        + path_heap + authority_heap + body_heap + headers_heap
        + 32; // padding

    Context *ctx = context_new(glb);
    if (!ctx) {
        send_error_response(response_out, 500, "Failed to create AtomVM context");
        goto cleanup_request;
    }

    if (UNLIKELY(memory_ensure_free(ctx, total_heap) != MEMORY_GC_OK)) {
        send_error_response(response_out, 500, "AtomVM out of memory");
        context_destroy(ctx);
        goto cleanup_request;
    }

    // Build the request map
    term method_atom = method_to_atom(glb, &method);

    term path_term;
    if (has_path) {
        path_term = term_from_literal_binary(
            (const void *) path_str.ptr, path_str.len, &ctx->heap, glb);
    } else {
        path_term = term_from_literal_binary("/", 1, &ctx->heap, glb);
    }

    term authority_term = term_invalid_term();
    if (has_authority) {
        authority_term = term_from_literal_binary(
            (const void *) authority_str.ptr, authority_str.len, &ctx->heap, glb);
    }

    term body_term = term_from_literal_binary(
        (const void *) body_data, body_len, &ctx->heap, glb);

    // Build headers list: [{Name :: binary(), Value :: binary()}]
    term headers_list = term_nil();
    // Build in reverse order (prepend)
    for (int i = (int) header_entries.len - 1; i >= 0; i--) {
        term name_bin = term_from_literal_binary(
            (const void *) header_entries.ptr[i].f0.ptr,
            header_entries.ptr[i].f0.len,
            &ctx->heap, glb);
        term value_bin = term_from_literal_binary(
            (const void *) header_entries.ptr[i].f1.ptr,
            header_entries.ptr[i].f1.len,
            &ctx->heap, glb);
        term tuple = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(tuple, 0, name_bin);
        term_put_tuple_element(tuple, 1, value_bin);
        headers_list = term_list_prepend(tuple, headers_list, &ctx->heap);
    }

    // Create the map
    // Keys: method, path, headers, body [, authority]
    term method_key = globalcontext_make_atom(glb, ATOM_STR("\x6", "method"));
    term path_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "path"));
    term headers_key = globalcontext_make_atom(glb, ATOM_STR("\x7", "headers"));
    term body_key = globalcontext_make_atom(glb, ATOM_STR("\x4", "body"));

    term req_map = term_alloc_map(num_map_keys, &ctx->heap);
    term_set_map_assoc(req_map, 0, body_key, body_term);
    term_set_map_assoc(req_map, 1, headers_key, headers_list);
    term_set_map_assoc(req_map, 2, method_key, method_atom);
    term_set_map_assoc(req_map, 3, path_key, path_term);
    if (has_authority) {
        term authority_key = globalcontext_make_atom(glb, ATOM_STR("\x9", "authority"));
        term_set_map_assoc(req_map, 4, authority_key, authority_term);
    }

    // -----------------------------------------------------------------------
    // 3. Call the handler: spin_handler:handle/1 or Elixir.SpinHandler:handle/1
    // -----------------------------------------------------------------------

    // Look up the handler module — try Erlang name first, then Elixir name
    Module *handler_mod = NULL;

    // Try: spin_handler (Erlang)
    term erl_mod_atom = globalcontext_make_atom(glb, ATOM_STR("\xC", "spin_handler"));
    handler_mod = globalcontext_get_module(glb, term_to_atom_index(erl_mod_atom));

    if (!handler_mod) {
        // Try: Elixir.SpinHandler (Elixir)
        term ex_mod_atom = globalcontext_make_atom(glb, ATOM_STR("\x12", "Elixir.SpinHandler"));
        handler_mod = globalcontext_get_module(glb, term_to_atom_index(ex_mod_atom));
    }

    if (!handler_mod) {
        send_error_response(response_out, 500,
            "Handler module not found. "
            "Define spin_handler:handle/1 (Erlang) or SpinHandler.handle/1 (Elixir)");
        context_destroy(ctx);
        goto cleanup_request;
    }

    // Set x[0] = request map
    ctx->x[0] = req_map;

    // Execute handle/1
    int exec_err = context_execute_loop(ctx, handler_mod, "handle", 1);

    if (exec_err) {
        send_error_response(response_out, 500, "Handler execution error");
        context_destroy(ctx);
        goto cleanup_request;
    }

    term result = ctx->x[0];

    // -----------------------------------------------------------------------
    // 4. Parse the Erlang response and send it via WASI
    //
    // Expected format:
    //   #{status => integer(), headers => [{binary(), binary()}], body => binary()}
    //
    // Or a simple binary (treated as 200 OK with text/plain body)
    // -----------------------------------------------------------------------

    uint16_t resp_status = 200;
    const char *resp_body_ptr = NULL;
    size_t resp_body_len = 0;
    term resp_headers_term = term_nil();

    if (term_is_map(result)) {
        // Extract status
        term status_key = globalcontext_make_atom(glb, ATOM_STR("\x6", "status"));
        term status_val = term_get_map_assoc(result, status_key, glb);
        if (term_is_integer(status_val)) {
            resp_status = (uint16_t) term_to_int(status_val);
        }

        // Extract body
        term resp_body_val = term_get_map_assoc(result, body_key, glb);
        if (term_is_binary(resp_body_val)) {
            resp_body_ptr = term_binary_data(resp_body_val);
            resp_body_len = term_binary_size(resp_body_val);
        }

        // Extract headers
        resp_headers_term = term_get_map_assoc(result, headers_key, glb);
        if (term_is_invalid_term(resp_headers_term)) {
            resp_headers_term = term_nil();
        }
    } else if (term_is_binary(result)) {
        // Simple binary response
        resp_body_ptr = term_binary_data(result);
        resp_body_len = term_binary_size(result);
    } else {
        // Unrecognized response format
        send_error_response(response_out, 500, "Invalid handler response format");
        context_destroy(ctx);
        goto cleanup_request;
    }

    // Build WASI response
    // Create response headers
    wasi_http_types_own_fields_t wasi_resp_headers = wasi_http_types_constructor_fields();
    wasi_http_types_borrow_fields_t wasi_resp_headers_borrow
        = wasi_http_types_borrow_fields(wasi_resp_headers);

    // Add headers from Erlang response
    if (term_is_nonempty_list(resp_headers_term)) {
        term cursor = resp_headers_term;
        while (term_is_nonempty_list(cursor)) {
            term entry = term_get_list_head(cursor);
            if (term_is_tuple(entry) && term_get_tuple_arity(entry) == 2) {
                term hdr_name = term_get_tuple_element(entry, 0);
                term hdr_val = term_get_tuple_element(entry, 1);
                if (term_is_binary(hdr_name) && term_is_binary(hdr_val)) {
                    wasi_http_types_field_name_t wasi_name = {
                        .ptr = (uint8_t *) term_binary_data(hdr_name),
                        .len = term_binary_size(hdr_name)
                    };
                    wasi_http_types_field_value_t wasi_val = {
                        .ptr = (uint8_t *) term_binary_data(hdr_val),
                        .len = term_binary_size(hdr_val)
                    };
                    wasi_http_types_header_error_t hdr_err;
                    wasi_http_types_method_fields_append(
                        wasi_resp_headers_borrow, &wasi_name, &wasi_val, &hdr_err);
                }
            }
            cursor = term_get_list_tail(cursor);
        }
    }

    // If no content-type header was set, default to text/plain
    {
        wasi_http_types_field_name_t ct_check_name;
        app_string_set(&ct_check_name, "content-type");
        if (!wasi_http_types_method_fields_has(wasi_resp_headers_borrow, &ct_check_name)) {
            wasi_http_types_field_value_t ct_val = {
                .ptr = (uint8_t *) "text/plain; charset=utf-8",
                .len = 25
            };
            wasi_http_types_header_error_t ct_err;
            wasi_http_types_method_fields_append(
                wasi_resp_headers_borrow, &ct_check_name, &ct_val, &ct_err);
        }
    }

    // Create outgoing response
    wasi_http_types_own_outgoing_response_t wasi_response
        = wasi_http_types_constructor_outgoing_response(wasi_resp_headers);
    wasi_http_types_borrow_outgoing_response_t wasi_resp_borrow
        = wasi_http_types_borrow_outgoing_response(wasi_response);
    wasi_http_types_method_outgoing_response_set_status_code(wasi_resp_borrow, resp_status);

    // Write body
    wasi_http_types_own_outgoing_body_t wasi_resp_body;
    if (wasi_http_types_method_outgoing_response_body(wasi_resp_borrow, &wasi_resp_body)) {
        wasi_http_types_borrow_outgoing_body_t body_borrow
            = wasi_http_types_borrow_outgoing_body(wasi_resp_body);
        wasi_http_types_own_output_stream_t out_stream;
        if (wasi_http_types_method_outgoing_body_write(body_borrow, &out_stream)) {
            if (resp_body_ptr && resp_body_len > 0) {
                wasi_io_streams_borrow_output_stream_t stream_borrow
                    = wasi_io_streams_borrow_output_stream(out_stream);
                app_list_u8_t body_bytes = {
                    .ptr = (uint8_t *) resp_body_ptr,
                    .len = resp_body_len
                };
                wasi_io_streams_stream_error_t stream_err;
                wasi_io_streams_method_output_stream_blocking_write_and_flush(
                    stream_borrow, &body_bytes, &stream_err);
            }
            wasi_io_streams_output_stream_drop_own(out_stream);
        }
        wasi_http_types_error_code_t finish_err;
        wasi_http_types_own_trailers_t *no_trailers = NULL;
        wasi_http_types_static_outgoing_body_finish(wasi_resp_body, no_trailers, &finish_err);
    }

    // Set the response on the outparam
    wasi_http_types_result_own_outgoing_response_error_code_t resp_result;
    resp_result.is_err = false;
    resp_result.val.ok = wasi_response;
    wasi_http_types_static_response_outparam_set(response_out, &resp_result);

    context_destroy(ctx);

cleanup_request:
    // Free C-side allocations
    wasi_http_types_method_free(&method);
    if (has_path) app_string_free(&path_str);
    if (has_authority) app_string_free(&authority_str);
    wasi_http_types_fields_drop_own(headers_handle);
    app_list_tuple2_field_name_field_value_free(&header_entries);
    free(body_data);
    wasi_http_types_incoming_request_drop_own(request);
}
