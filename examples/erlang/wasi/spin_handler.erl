%% @doc Spin HTTP handler for AtomVM.
%%
%% This module implements the handler callback for the wasi:http/incoming-handler
%% integration. When Spin (or wasmtime serve) receives an HTTP request, AtomVM
%% calls `handle/1` with a request map and expects a response map back.
%%
%% == Request Map ==
%% The request map has the following keys:
%% <ul>
%%   <li>`method' - atom: get, post, put, delete, head, options, patch, trace, connect</li>
%%   <li>`path' - binary: the request path with query string, e.g. &lt;&lt;"/hello?name=world"&gt;&gt;</li>
%%   <li>`headers' - list of {Name :: binary(), Value :: binary()} tuples</li>
%%   <li>`body' - binary: the request body (empty binary if no body)</li>
%%   <li>`authority' - binary: the host header value (optional)</li>
%% </ul>
%%
%% == Response Map ==
%% The response map should have:
%% <ul>
%%   <li>`status' - integer: HTTP status code (e.g. 200)</li>
%%   <li>`headers' - list of {Name :: binary(), Value :: binary()} tuples</li>
%%   <li>`body' - binary: the response body</li>
%% </ul>
%%
%% Alternatively, returning a plain binary is treated as a 200 OK response
%% with content-type text/plain.

-module(spin_handler).
-export([handle/1]).

%% @doc Handle an incoming HTTP request.
%% @param Request A map containing the HTTP request data.
%% @returns A map containing the HTTP response data.
handle(#{method := Method, path := Path, headers := Headers, body := Body} = _Request) ->
    %% Route based on path
    case Path of
        <<"/">> ->
            respond_ok(<<"Hello from AtomVM on Spin!">>);
        <<"/hello">> ->
            respond_ok(<<"Hello, World!">>);
        <<"/echo">> ->
            %% Echo back the request body
            #{
                status => 200,
                headers => [{<<"content-type">>, <<"application/octet-stream">>}],
                body => Body
            };
        <<"/info">> ->
            %% Return request info as text
            Info = format_request_info(Method, Path, Headers, Body),
            #{
                status => 200,
                headers => [{<<"content-type">>, <<"text/plain; charset=utf-8">>}],
                body => Info
            };
        <<"/json">> ->
            %% Simple JSON response
            #{
                status => 200,
                headers => [{<<"content-type">>, <<"application/json">>}],
                body => <<"{\"message\":\"Hello from AtomVM\",\"platform\":\"wasi\"}">>
            };
        <<"/proxy">> ->
            %% Demonstrate outbound HTTP: fetch from httpbin and return the result
            case spin_http:get(<<"https://httpbin.org/get">>) of
                {ok, #{status := ProxyStatus, body := ProxyBody}} ->
                    StatusBin = integer_to_binary(ProxyStatus),
                    #{
                        status => 200,
                        headers => [{<<"content-type">>, <<"text/plain; charset=utf-8">>}],
                        body => <<"Upstream status: ", StatusBin/binary, "\n\n", ProxyBody/binary>>
                    };
                {error, Reason} ->
                    ErrBin = atom_to_binary(Reason, utf8),
                    #{
                        status => 502,
                        headers => [{<<"content-type">>, <<"text/plain">>}],
                        body => <<"Upstream request failed: ", ErrBin/binary>>
                    }
            end;
        <<"/fetch">> ->
            %% Fetch a URL passed as query parameter: /fetch?url=https://...
            %% For simplicity, just POST to httpbin echo
            case spin_http:post(
                <<"https://httpbin.org/post">>,
                [{<<"content-type">>, <<"application/json">>}],
                <<"{\"from\":\"AtomVM\",\"message\":\"Hello from BEAM on WASM!\"}">>
            ) of
                {ok, #{body := RespBody}} ->
                    #{
                        status => 200,
                        headers => [{<<"content-type">>, <<"application/json">>}],
                        body => RespBody
                    };
                {error, _} ->
                    #{
                        status => 502,
                        headers => [{<<"content-type">>, <<"text/plain">>}],
                        body => <<"Failed to reach upstream">>
                    }
            end;
        _ ->
            %% 404 for unknown paths
            #{
                status => 404,
                headers => [{<<"content-type">>, <<"text/plain">>}],
                body => <<"Not Found">>
            }
    end.

%% @doc Create a simple 200 OK text response.
respond_ok(Body) ->
    #{
        status => 200,
        headers => [{<<"content-type">>, <<"text/plain; charset=utf-8">>}],
        body => Body
    }.

%% @doc Format request information as a readable string.
format_request_info(Method, Path, Headers, Body) ->
    MethodBin = atom_to_binary(Method, utf8),
    HeaderLines = format_headers(Headers),
    BodyInfo = case byte_size(Body) of
        0 -> <<"(empty)">>;
        N ->
            SizeBin = integer_to_binary(N),
            <<SizeBin/binary, " bytes">>
    end,
    <<"Method: ", MethodBin/binary, "\n",
      "Path: ", Path/binary, "\n",
      "Headers:\n", HeaderLines/binary,
      "Body: ", BodyInfo/binary, "\n">>.

%% @doc Format headers as indented lines.
format_headers(Headers) ->
    format_headers(Headers, <<>>).

format_headers([], Acc) ->
    Acc;
format_headers([{Name, Value} | Rest], Acc) ->
    Line = <<"  ", Name/binary, ": ", Value/binary, "\n">>,
    format_headers(Rest, <<Acc/binary, Line/binary>>).
