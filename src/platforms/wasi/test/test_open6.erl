-module(test_open6).
-export([start/0]).

start() ->
    %% Exact flags from test_file_io, absolute path
    erlang:display(test1_wronly_creat_trunc_abs),
    R1 = (catch atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                    [o_wronly, o_creat, o_trunc],
                                    8#644)),
    erlang:display(R1),

    %% Same but relative path
    erlang:display(test2_wronly_creat_trunc_rel),
    R2 = (catch atomvm:posix_open("test_output2.txt",
                                    [o_wronly, o_creat, o_trunc],
                                    8#644)),
    erlang:display(R2),

    %% Without trunc, absolute path
    erlang:display(test3_wronly_creat_abs),
    R3 = (catch atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                    [o_wronly, o_creat],
                                    8#644)),
    erlang:display(R3),

    ok.
