-module(test_lists).
-export([start/0]).

start() ->
    %% List operations
    L1 = [1, 2, 3, 4, 5],
    L2 = [6, 7, 8],
    Concat = L1 ++ L2,
    erlang:display(Concat),

    %% List comprehension
    Squares = [X * X || X <- L1],
    erlang:display(Squares),

    %% Recursive length
    Len = length(L1),
    erlang:display(Len),

    %% Head/tail
    [H | T] = L1,
    erlang:display(H),
    erlang:display(T),

    %% Reverse (manual)
    Rev = reverse(L1),
    erlang:display(Rev),

    ok.

reverse(L) -> reverse(L, []).
reverse([], Acc) -> Acc;
reverse([H|T], Acc) -> reverse(T, [H|Acc]).
