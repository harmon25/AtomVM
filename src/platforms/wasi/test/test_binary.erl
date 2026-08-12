-module(test_binary).
-export([start/0]).

start() ->
    %% Binary construction
    B1 = <<1, 2, 3, 4, 5>>,
    erlang:display(B1),

    %% Binary size
    erlang:display(byte_size(B1)),

    %% Binary pattern matching
    <<A, B, Rest/binary>> = B1,
    erlang:display(A),
    erlang:display(B),
    erlang:display(Rest),

    %% String as binary
    Str = <<"Hello, WASI!">>,
    erlang:display(Str),

    %% Binary to list
    L = binary_to_list(B1),
    erlang:display(L),

    ok.
