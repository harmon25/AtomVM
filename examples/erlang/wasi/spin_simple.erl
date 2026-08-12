-module(spin_simple).
-export([start/0]).

start() ->
    % Simple computation without IO
    X = 2 + 2,
    Y = X * 10,
    % Return ok if computation succeeded
    case Y of
        40 -> ok;
        _ -> error
    end.