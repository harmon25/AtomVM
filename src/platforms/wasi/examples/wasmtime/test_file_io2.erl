-module(test_file_io2).
-export([start/0]).

start() ->
    %% Write a file using relative path
    erlang:display(writing_file),
    R1 = (catch atomvm:posix_open("test_output.txt",
                                   [o_wronly, o_creat, o_trunc],
                                   8#644)),
    erlang:display(R1),
    case R1 of
        {ok, Fd} ->
            W1 = atomvm:posix_write(Fd, <<"Hello from AtomVM on WASI!\n">>),
            erlang:display(W1),
            W2 = atomvm:posix_write(Fd, <<"Line 2: file I/O works.\n">>),
            erlang:display(W2),
            C1 = atomvm:posix_close(Fd),
            erlang:display(C1),

            %% Read it back
            erlang:display(reading_file),
            {ok, Fd2} = atomvm:posix_open("test_output.txt", [o_rdonly]),
            {ok, Data} = atomvm:posix_read(Fd2, 1024),
            ok = atomvm:posix_close(Fd2),
            erlang:display(Data),
            
            %% List directory
            erlang:display(listing_dir),
            R3 = (catch atomvm:posix_opendir(".")),
            erlang:display(R3),
            case R3 of
                {ok, Dir} ->
                    list_dir(Dir),
                    atomvm:posix_closedir(Dir);
                _ ->
                    erlang:display(opendir_failed)
            end;
        _ ->
            erlang:display(open_failed)
    end,

    erlang:display(done),
    ok.

list_dir(Dir) ->
    case atomvm:posix_readdir(Dir) of
        {ok, Entry} ->
            erlang:display(Entry),
            list_dir(Dir);
        error ->
            ok
    end.
