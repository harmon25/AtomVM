-module(test_maps).
-export([start/0]).

start() ->
    %% Map creation
    M1 = #{name => <<"AtomVM">>, platform => wasi, version => 1},
    erlang:display(M1),

    %% Map access
    Name = maps:get(name, M1),
    erlang:display(Name),

    %% Map update
    M2 = M1#{version => 2},
    erlang:display(M2),

    %% Map size
    erlang:display(maps:size(M2)),

    %% Map keys
    Keys = maps:keys(M1),
    erlang:display(Keys),

    ok.
