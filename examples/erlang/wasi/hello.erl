-module(hello).
-export([start/0]).

start() ->
    erlang:display(hello_world),
    ok.
