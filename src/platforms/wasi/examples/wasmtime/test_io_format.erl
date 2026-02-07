-module(test_io_format).
-export([start/0]).

start() ->
    io:format("Hello from io:format!~n"),
    io:format("Number: ~p~n", [42]),
    io:format("String: ~s~n", ["AtomVM on WASI"]),
    io:format("List: ~p~n", [[1, 2, 3]]),
    io:format("Atom: ~p~n", [hello]),
    io:format("Tuple: ~p~n", [{ok, <<"binary">>}]),
    io:format("Map: ~p~n", [#{a => 1, b => 2}]),
    ok.
