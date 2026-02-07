-module(test_console_port2).
-export([start/0]).

start() ->
    %% Open console port
    Port = open_port({spawn, "console"}, []),
    erlang:display({port, Port}),

    %% Test IO protocol directly
    Ref1 = make_ref(),
    Port ! {io_request, self(), Ref1, {put_chars, latin1, "Direct IO: hello!\n"}},
    receive
        {io_reply, Ref1, ok} -> erlang:display(io_direct_ok)
    after 1000 ->
        erlang:display(io_direct_timeout)
    end,

    %% Test io:format through group leader (PATH 2)
    OldGL = erlang:group_leader(),
    true = erlang:group_leader(Port, self()),
    io:format("PATH 2 io:format: ~p ~p~n", [hello, world]),
    true = erlang:group_leader(OldGL, self()),

    erlang:display(all_done),
    ok.
