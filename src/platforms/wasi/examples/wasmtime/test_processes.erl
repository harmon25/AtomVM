-module(test_processes).
-export([start/0]).

start() ->
    %% Spawn a process
    Self = self(),
    erlang:display(Self),

    Pid = spawn(fun() -> Self ! {hello, self()} end),
    erlang:display(Pid),

    receive
        {hello, From} ->
            erlang:display(From),
            erlang:display(got_message)
    after 5000 ->
        erlang:display(timeout)
    end,

    %% Spawn multiple and collect
    Pids = [spawn(fun() -> Self ! {result, N * N} end) || N <- [1, 2, 3]],
    erlang:display(length(Pids)),

    collect(3),

    ok.

collect(0) -> ok;
collect(N) ->
    receive
        {result, Val} ->
            erlang:display(Val)
    after 5000 ->
        erlang:display(timeout)
    end,
    collect(N - 1).
