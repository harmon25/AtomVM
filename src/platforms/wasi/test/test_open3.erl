-module(test_open3).
-export([start/0]).

start() ->
    %% Try calling posix_open/3 directly with simplest possible args
    erlang:display(calling_posix_open_3),
    R = (catch atomvm:posix_open("test.txt", [o_wronly, o_creat], 420)),
    erlang:display(R),

    %% Now try posix_open/2 without o_creat
    erlang:display(calling_posix_open_2),
    R2 = (catch atomvm:posix_open("test.txt", [o_rdonly])),
    erlang:display(R2),

    ok.
