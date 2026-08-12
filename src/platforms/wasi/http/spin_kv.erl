%% @doc Key-Value store client for AtomVM on Spin.
%%
%% Provides access to Spin's built-in key-value store via
%% fermyon:spin/key-value. Values are stored as binaries.
%%
%% == Usage ==
%% ```
%% {ok, Store} = spin_kv:open(<<"default">>),
%% ok = spin_kv:set(Store, <<"key">>, <<"value">>),
%% {ok, <<"value">>} = spin_kv:get(Store, <<"key">>),
%% ok = spin_kv:delete(Store, <<"key">>),
%% ok = spin_kv:close(Store).
%% '''
%%
%% Configure access in spin.toml:
%% ```
%% [component.atomvm]
%% key_value_stores = ["default"]
%% '''

-module(spin_kv).
-export([open/1, get/2, set/3, delete/2, exists/2, get_keys/1, close/1]).

%% @doc Open a key-value store by label.
-spec open(binary()) -> {ok, term()} | {error, term()}.
open(_Label) -> erlang:nif_error(undefined).

%% @doc Get a value by key. Returns {ok, Binary} or {ok, undefined} if not found.
-spec get(term(), binary()) -> {ok, binary() | undefined} | {error, term()}.
get(_Store, _Key) -> erlang:nif_error(undefined).

%% @doc Set a key-value pair.
-spec set(term(), binary(), binary()) -> ok | {error, term()}.
set(_Store, _Key, _Value) -> erlang:nif_error(undefined).

%% @doc Delete a key.
-spec delete(term(), binary()) -> ok | error.
delete(_Store, _Key) -> erlang:nif_error(undefined).

%% @doc Check if a key exists.
-spec exists(term(), binary()) -> boolean().
exists(_Store, _Key) -> erlang:nif_error(undefined).

%% @doc List all keys in the store.
-spec get_keys(term()) -> {ok, [binary()]} | {error, term()}.
get_keys(_Store) -> erlang:nif_error(undefined).

%% @doc Close the store handle.
-spec close(term()) -> ok.
close(_Store) -> erlang:nif_error(undefined).
