-module(test_file_io).
-export([start/0]).

start() ->
    %% Write a file
    erlang:display(writing_file),
    {ok, Fd} = atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                  [o_wronly, o_creat, o_trunc],
                                  8#644),
    {ok, _} = atomvm:posix_write(Fd, <<"Hello from AtomVM on WASI!\n">>),
    {ok, _} = atomvm:posix_write(Fd, <<"Line 2: file I/O works.\n">>),
    ok = atomvm:posix_close(Fd),
    erlang:display(file_written),

    %% Read it back
    erlang:display(reading_file),
    {ok, Fd2} = atomvm:posix_open("/tmp/atomvm-test/test_output.txt",
                                   [o_rdonly]),
    {ok, Data} = atomvm:posix_read(Fd2, 1024),
    ok = atomvm:posix_close(Fd2),
    erlang:display(Data),

    %% Get current working directory
    {ok, Cwd} = file:get_cwd(),
    erlang:display(Cwd),

    %% List a directory
    erlang:display(listing_dir),
    DirResult = (catch list_directory("/tmp/atomvm-test")),
    erlang:display(DirResult),

    erlang:display(done),
    ok.

list_directory(Path) ->
    {ok, Dir} = atomvm:posix_opendir(Path),
    list_dir(Dir),
    ok = atomvm:posix_closedir(Dir),
    dir_listed.

list_dir(Dir) ->
    case atomvm:posix_readdir(Dir) of
        {ok, Entry} ->
            erlang:display(Entry),
            list_dir(Dir);
        eof ->
            ok;
        {error, Reason} ->
            erlang:display({readdir_error, Reason}),
            ok
    end.
