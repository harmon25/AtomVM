-module(test_console_port).
-export([start/0]).

start() ->
    %% Test opening a console port
    erlang:display(opening_console_port),
    Port = open_port({spawn, "console"}, []),
    erlang:display({port_opened, Port}),

    %% Send a message through the port (gen_server-style puts)
    erlang:display(sending_puts),
    Ref = make_ref(),
    Port ! {'$gen_call', {self(), Ref}, {puts, "Hello via console port!\n"}},
    receive
        {Ref, Reply} -> erlang:display({puts_reply, Reply})
    after 1000 ->
        erlang:display(puts_timeout)
    end,

    %% Send via IO protocol
    erlang:display(sending_io_request),
    Ref2 = make_ref(),
    Port ! {io_request, self(), Ref2, {put_chars, latin1, "IO protocol works!\n"}},
    receive
        {io_reply, Ref2, Reply2} -> erlang:display({io_reply, Reply2})
    after 1000 ->
        erlang:display(io_reply_timeout)
    end,

    %% Test io:format with explicit group leader set to console port
    %% (This tests PATH 2 of io:format)
    erlang:display(testing_io_format_via_port),
    OldGL = erlang:group_leader(),
    true = erlang:group_leader(Port, self()),
    io:format("io:format via console port: ~p~n", [works]),
    true = erlang:group_leader(OldGL, self()),

    %% Close the port
    erlang:display(closing_port),
    Port ! {self(), close},
    receive
        {Port, closed} -> erlang:display(port_closed)
    after 1000 ->
        erlang:display(close_timeout)
    end,

    erlang:display(all_done),
    ok.
