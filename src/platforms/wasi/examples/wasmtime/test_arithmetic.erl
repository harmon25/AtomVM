-module(test_arithmetic).
-export([start/0]).

start() ->
    %% Basic integer arithmetic
    A = 42,
    B = 13,
    Sum = A + B,
    Diff = A - B,
    Prod = A * B,
    Quot = A div B,
    Rem = A rem B,

    erlang:display(Sum),    %% 55
    erlang:display(Diff),   %% 29
    erlang:display(Prod),   %% 546
    erlang:display(Quot),   %% 3
    erlang:display(Rem),    %% 3

    %% Float arithmetic
    Pi = 3.14159,
    erlang:display(Pi),

    %% Big integers
    Big = 1 bsl 64,
    erlang:display(Big),

    ok.
