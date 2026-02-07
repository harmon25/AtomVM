-module(spin_handler).
-export([start/0]).

start() ->
    io:format("🚀 Erlang running on Fermyon Spin!~n~n"),
    io:format("This is an Erlang HTTP handler running as a Spin component.~n"),
    io:format("Features demonstrated:~n"),
    io:format("  ✓ Process spawning: ~p~n", [self()]),
    io:format("  ✓ List operations: ~p~n", [[1,2,3] ++ [4,5,6]]),
    io:format("  ✓ Pattern matching: ~p~n", [ classify(42) ]),
    io:format("  ✓ Recursion: fib(10) = ~p~n", [ fib(10) ]),
    io:format("~nHello from Erlang on Spin! 🎉~n"),
    ok.

classify(N) when N > 0 -> positive;
classify(N) when N < 0 -> negative;
classify(0) -> zero.

fib(0) -> 0;
fib(1) -> 1;
fib(N) -> fib(N-1) + fib(N-2).