-module(test_socket_basic).
-export([start/0]).

start() ->
    io:format("Testing basic socket operations...~n"),
    
    % Test 1: Open a TCP socket
    io:format("Test 1: Opening TCP socket... "),
    case socket:open(inet, stream, tcp) of
        {ok, Socket} ->
            io:format("OK~n"),
            
            % Test 2: Get socket info
            io:format("Test 2: Getting socket info... "),
            case socket:sockname(Socket) of
                {ok, #{addr := Addr, port := Port}} ->
                    io:format("OK (addr=~p, port=~p)~n", [Addr, Port]);
                {error, Reason} ->
                    io:format("FAILED: ~p~n", [Reason])
            end,
            
            % Test 3: Close socket
            io:format("Test 3: Closing socket... "),
            case socket:close(Socket) of
                ok ->
                    io:format("OK~n");
                {error, CloseReason} ->
                    io:format("FAILED: ~p~n", [CloseReason])
            end;
        {error, OpenReason} ->
            io:format("FAILED: ~p~n", [OpenReason])
    end,
    
    % Test 4: Open a UDP socket
    io:format("Test 4: Opening UDP socket... "),
    case socket:open(inet, dgram, udp) of
        {ok, UdpSocket} ->
            io:format("OK~n"),
            socket:close(UdpSocket),
            io:format("All basic tests passed!~n");
        {error, UdpReason} ->
            io:format("FAILED: ~p~n", [UdpReason])
    end,
    
    ok.