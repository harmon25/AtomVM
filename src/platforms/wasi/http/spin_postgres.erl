%% @doc PostgreSQL client for AtomVM on Spin.
%%
%% Provides access to PostgreSQL databases via fermyon:spin/postgres.
%%
%% == Usage ==
%% ```
%% {ok, Conn} = spin_postgres:open(<<"host=localhost dbname=mydb">>),
%% {ok, Result} = spin_postgres:query(Conn, <<"SELECT * FROM users WHERE id = $1">>, [42]),
%% #{columns := Columns, rows := Rows} = Result,
%% {ok, RowsAffected} = spin_postgres:execute(Conn, <<"DELETE FROM users WHERE id = $1">>, [42]),
%% ok = spin_postgres:close(Conn).
%% '''
%%
%% == Result Format (query) ==
%% ```
%% #{columns => [<<"id">>, <<"name">>],
%%   rows => [[42, <<"alice">>]]}
%% '''
%%
%% == Parameter Types ==
%% - integer() -> INT64
%% - float()   -> FLOAT64
%% - binary()  -> TEXT
%% - true/false -> BOOLEAN
%% - null/undefined -> NULL
%%
%% Configure access in spin.toml:
%% ```
%% [component.atomvm]
%% allowed_outbound_hosts = ["postgres://localhost:5432"]
%% '''

-module(spin_postgres).
-export([open/1, query/2, query/3, execute/2, execute/3, close/1]).

%% @doc Open a PostgreSQL connection.
-spec open(binary()) -> {ok, term()} | {error, term()}.
open(_Address) -> erlang:nif_error(undefined).

%% @doc Execute a query with no parameters. Returns column names and rows.
-spec query(term(), binary()) -> {ok, map()} | {error, term()}.
query(_Conn, _Statement) -> erlang:nif_error(undefined).

%% @doc Execute a query with parameters.
-spec query(term(), binary(), [term()]) -> {ok, map()} | {error, term()}.
query(_Conn, _Statement, _Params) -> erlang:nif_error(undefined).

%% @doc Execute a command with no parameters. Returns rows affected.
-spec execute(term(), binary()) -> {ok, integer()} | {error, term()}.
execute(_Conn, _Statement) -> erlang:nif_error(undefined).

%% @doc Execute a command with parameters. Returns rows affected.
-spec execute(term(), binary(), [term()]) -> {ok, integer()} | {error, term()}.
execute(_Conn, _Statement, _Params) -> erlang:nif_error(undefined).

%% @doc Close the PostgreSQL connection.
-spec close(term()) -> ok.
close(_Conn) -> erlang:nif_error(undefined).
