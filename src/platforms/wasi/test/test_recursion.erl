-module(test_recursion).
-export([start/0]).

start() ->
    %% Fibonacci
    Fib10 = fib(10),
    erlang:display(Fib10),   %% 55

    Fib20 = fib(20),
    erlang:display(Fib20),   %% 6765

    %% Factorial
    Fact10 = factorial(10),
    erlang:display(Fact10),  %% 3628800

    %% Ackermann (small values)
    Ack = ackermann(3, 4),
    erlang:display(Ack),     %% 125

    ok.

fib(0) -> 0;
fib(1) -> 1;
fib(N) when N > 1 -> fib(N-1) + fib(N-2).

factorial(0) -> 1;
factorial(N) when N > 0 -> N * factorial(N-1).

ackermann(0, N) -> N + 1;
ackermann(M, 0) when M > 0 -> ackermann(M - 1, 1);
ackermann(M, N) when M > 0, N > 0 -> ackermann(M - 1, ackermann(M, N - 1)).
