%% @doc Application configuration / variables for AtomVM on Spin.
%%
%% Provides access to application variables defined in spin.toml via
%% wasi:config/store. Use this for API keys, secrets, feature flags,
%% and any configuration that should be separate from code.
%%
%% == Usage ==
%% ```
%% {ok, ApiKey} = spin_config:get(<<"api_key">>),
%% {ok, AllVars} = spin_config:get_all().
%% '''
%%
%% == spin.toml ==
%% ```
%% [variables]
%% api_key = { required = true }
%% log_level = { default = "info" }
%%
%% [component.atomvm]
%% # Variables are automatically available to the component
%% '''
%%
%% == Setting values at runtime ==
%% ```
%% spin up --variable api_key=secret123
%% '''

-module(spin_config).
-export([get/1, get_all/0]).

%% @doc Get a configuration value by key.
%% Returns {ok, Binary} if found, {ok, undefined} if not set.
-spec get(binary()) -> {ok, binary() | undefined} | {error, term()}.
get(_Key) -> erlang:nif_error(undefined).

%% @doc Get all configuration key-value pairs.
%% Returns {ok, [{Key :: binary(), Value :: binary()}]}.
-spec get_all() -> {ok, [{binary(), binary()}]} | {error, term()}.
get_all() -> erlang:nif_error(undefined).
