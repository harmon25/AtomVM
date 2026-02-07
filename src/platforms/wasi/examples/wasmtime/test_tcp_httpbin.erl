-module(test_tcp_httpbin).
-export([start/0]).

start() ->
    io:format("TCP HTTP Request Test~n"),
    io:format("=====================~n~n"),
    
    % httpbin.org IP (hardcoded)
    Ip = {54,208,3,199},
    Port = 80,
    
    io:format("1. Opening socket... "),
    {ok, Socket} = socket:open(inet, stream, tcp),
    io:format("OK~n"),
    
    io:format("2. Connecting to ~p:~p... ", [Ip, Port]),
    case socket:connect(Socket, #{family => inet, addr => Ip, port => Port}) of
        ok ->
            io:format("OK~n"),
            send_http_request(Socket);
        {error, Reason} ->
            io:format("FAILED: ~p~n", [Reason]),
            socket:close(Socket)
    end.

send_http_request(Socket) ->
    io:format("3. Sending HTTP GET... "),
    HttpRequest = <<"GET /ip HTTP/1.0\r\nHost: httpbin.org\r\nUser-Agent: AtomVM-WASI\r\n\r\n">>,
    case socket:send(Socket, HttpRequest) of
        {ok, Sent} ->
            io:format("OK (~p bytes)~n", [Sent]),
            receive_response(Socket);
        {error, Err} ->
            io:format("SEND FAILED: ~p~n", [Err]),
            socket:close(Socket)
    end.

receive_response(Socket) ->
    io:format("4. Receiving response... "),
    case socket:recv(Socket, 0, 10000) of
        {ok, Data} ->
            io:format("OK (~p bytes)~n~n", [byte_size(Data)]),
            io:format("=== Response ===~n"),
            io:format("~s~n", [Data]),
            socket:close(Socket),
            io:format("~n5. Socket closed. Test complete!~n");
        {error, Timeout} when Timeout == timeout; Timeout == etimedout ->
            io:format("TIMEOUT~n"),
            socket:close(Socket);
        {error, RecvErr} ->
            io:format("RECV ERROR: ~p~n", [RecvErr]),
            socket:close(Socket)
    end.