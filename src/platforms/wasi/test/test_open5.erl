-module(test_open5).
-export([start/0]).

start() ->
    %% Without o_trunc
    erlang:display(test1_no_trunc),
    R1 = (catch atomvm:posix_open("test_a.txt", [o_wronly, o_creat], 8#644)),
    erlang:display(R1),

    %% With o_trunc
    erlang:display(test2_with_trunc),
    R2 = (catch atomvm:posix_open("test_b.txt", [o_wronly, o_creat, o_trunc], 8#644)),
    erlang:display(R2),

    %% With only o_trunc and o_wronly (no o_creat) - file must exist
    erlang:display(test3_trunc_no_creat),
    R3 = (catch atomvm:posix_open("test_a.txt", [o_wronly, o_trunc])),
    erlang:display(R3),

    ok.
