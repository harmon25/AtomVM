-module(test_tcp_local).
-export([start/0]).

start() ->
    io:format("TCP Localhost Test~n"),
    io:format("==================~n~n"),
    
    io:format("1. Opening TCP socket... "),
    {ok, Socket} = socket:open(inet, stream, tcp),
    io:format("OK~n"),
    
    % Try connecting to localhost:12345 (should fail quickly if no server)
    io:format("2. Connecting to localhost:12345 (expecting refused)... "),
    case socket:connect(Socket, #{family => inet, addr => {127,0,0,1}, port => 12345}) of
        ok ->
            io:format("CONNECTED (unexpected - no server?)~n");
        {error, econnrefused} ->
            io:format("REFUSED (expected - no server running)~n");
        {error, Other} ->
            io:format("ERROR: ~p~n", [Other])
    end,
    
    socket:close(Socket),
    io:format("3. Socket closed~n"),
    ok.