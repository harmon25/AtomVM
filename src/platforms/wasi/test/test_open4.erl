-module(test_open4).
-export([start/0]).

start() ->
    %% Same as debug test but with absolute paths  
    erlang:display(test1_abs_path_creat),
    R1 = (catch atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                    [o_wronly, o_creat, o_trunc],
                                    8#644)),
    erlang:display(R1),

    %% With relative path
    erlang:display(test2_rel_path_creat),
    R2 = (catch atomvm:posix_open("test_output.txt",
                                    [o_wronly, o_creat, o_trunc],
                                    8#644)),
    erlang:display(R2),

    ok.
