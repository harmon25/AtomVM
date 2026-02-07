-module(test_file_debug).
-export([start/0]).

start() ->
    erlang:display(test_starting),

    %% Test 1: Try open with o_creat (3-arg form, absolute path)
    erlang:display(test1_open_creat_abs),
    Result1 = (catch atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                        [o_wronly, o_creat, o_trunc],
                                        8#644)),
    erlang:display(Result1),

    %% Test 2: Try open with o_creat (3-arg, relative path)
    erlang:display(test2_open_creat_rel),
    Result2 = (catch atomvm:posix_open("test_output2.txt",
                                        [o_wronly, o_creat, o_trunc],
                                        8#644)),
    erlang:display(Result2),

    %% Test 3: Try open without o_creat (read existing file)
    erlang:display(test3_open_rdonly),
    Result3 = (catch atomvm:posix_open("/tmp/atomvm-test/test_file_debug.erl",
                                        [o_rdonly])),
    erlang:display(Result3),

    %% Test 4: Try getcwd
    erlang:display(test4_getcwd),
    Result4 = (catch file:get_cwd()),
    erlang:display(Result4),

    erlang:display(all_done),
    ok.
