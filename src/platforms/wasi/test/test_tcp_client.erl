-module(test_tcp_client).
-export([start/0]).

start() ->
    io:format("TCP Client Test~n"),
    io:format("===============~n~n"),
    
    % Open a TCP socket
    io:format("1. Opening TCP socket... "),
    {ok, Socket} = socket:open(inet, stream, tcp),
    io:format("OK (fd assigned)~n"),
    
    % Connect to a test server (httpbin.org:80)
    io:format("2. Connecting to httpbin.org:80... "),
    case socket:connect(Socket, #{family => inet, addr => {54,208,3,199}, port => 80}) of
        ok ->
            io:format("OK~n"),
            
            % Get local and remote addresses
            io:format("3. Getting socket addresses... "),
            {ok, Local} = socket:sockname(Socket),
            {ok, Remote} = socket:peername(Socket),
            io:format("OK~n   Local: ~p~n   Remote: ~p~n", [Local, Remote]),
            
            % Send HTTP request
            io:format("4. Sending HTTP request... "),
            Request = <<"GET /get HTTP/1.0\r\nHost: httpbin.org\r\n\r\n">>,
            case socket:send(Socket, Request) of
                {ok, BytesSent} ->
                    io:format("OK (~p bytes sent)~n", [BytesSent]),
                    
                    % Receive response
                    io:format("5. Receiving response... "),
                    case socket:recv(Socket, 0, 5000) of
                        {ok, Data} ->
                            io:format("OK (~p bytes received)~n", [byte_size(Data)]),
                            io:format("~n--- Response (first 200 bytes) ---~n"),
                            io:format("~s...~n", [binary:part(Data, 0, min(200, byte_size(Data)))])
                    end;
                {error, SendErr} ->
                    io:format("FAILED: ~p~n", [SendErr])
            end,
            
            % Shutdown and close
            io:format("6. Shutting down socket... "),
            socket:shutdown(Socket, read_write),
            io:format("OK~n"),
            
            io:format("7. Closing socket... "),
            socket:close(Socket),
            io:format("OK~n");
            
        {error, ConnErr} ->
            io:format("FAILED: ~p~n", [ConnErr]),
            socket:close(Socket)
    end,
    
    io:format("~nTCP client test complete.~n"),
    ok.