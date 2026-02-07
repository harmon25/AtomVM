%% @doc SQLite database client for AtomVM on Spin.
%%
%% Provides access to Spin's built-in SQLite database via
%% fermyon:spin/sqlite.
%%
%% == Usage ==
%% ```
%% {ok, Conn} = spin_sqlite:open(<<"default">>),
%% {ok, Result} = spin_sqlite:execute(Conn, <<"SELECT * FROM users WHERE id = ?">>, [1]),
%% #{columns := Columns, rows := Rows} = Result,
%% ok = spin_sqlite:close(Conn).
%% '''
%%
%% == Result Format ==
%% ```
%% #{columns => [<<"id">>, <<"name">>, <<"email">>],
%%   rows => [[1, <<"alice">>, <<"alice@example.com">>],
%%            [2, <<"bob">>,   <<"bob@example.com">>]]}
%% '''
%%
%% == Parameter Types ==
%% - integer() -> INTEGER
%% - float()   -> REAL
%% - binary()  -> TEXT
%% - null atom -> NULL
%%
%% Configure access in spin.toml:
%% ```
%% [component.atomvm]
%% sqlite_databases = ["default"]
%% '''

-module(spin_sqlite).
-export([open/1, execute/2, execute/3, close/1]).

%% @doc Open a SQLite database connection by name.
-spec open(binary()) -> {ok, term()} | {error, term()}.
open(_Database) -> erlang:nif_error(undefined).

%% @doc Execute a SQL statement with no parameters.
-spec execute(term(), binary()) -> {ok, map()} | {error, term()}.
execute(_Conn, _Statement) -> erlang:nif_error(undefined).

%% @doc Execute a SQL statement with parameters.
-spec execute(term(), binary(), [term()]) -> {ok, map()} | {error, term()}.
execute(_Conn, _Statement, _Params) -> erlang:nif_error(undefined).

%% @doc Close the database connection.
-spec close(term()) -> ok.
close(_Conn) -> erlang:nif_error(undefined).
