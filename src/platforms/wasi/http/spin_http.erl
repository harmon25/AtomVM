%% @doc Outbound HTTP client for AtomVM on Spin/WASI.
%%
%% This module provides HTTP client functionality via the
%% wasi:http/outgoing-handler interface. It is available when running
%% inside the AtomVM HTTP component (AtomVM_http.wasm).
%%
%% == Basic Usage ==
%%
%% ```
%% {ok, Response} = spin_http:get(<<"https://api.example.com/data">>).
%% #{status := 200, body := Body} = Response.
%% '''
%%
%% == Full Request ==
%%
%% ```
%% {ok, Response} = spin_http:request(#{
%%     method  => post,
%%     url     => <<"https://api.example.com/items">>,
%%     headers => [{<<"content-type">>, <<"application/json">>}],
%%     body    => <<"{\"name\":\"test\"}">>
%% }).
%% '''
%%
%% == Response Map ==
%%
%% ```
%% #{status  => 200,
%%   headers => [{<<"content-type">>, <<"application/json">>}],
%%   body    => <<"...">>}
%% '''

-module(spin_http).
-export([
    request/1,
    get/1, get/2,
    post/3,
    put/3,
    delete/1, delete/2
]).

%% @doc Make an HTTP request.
%% @param Request A map: #{method => atom(), url => binary(),
%%   headers => [{binary(), binary()}], body => binary()}
%% @returns {ok, #{status => integer(), headers => [{binary(), binary()}],
%%   body => binary()}} | {error, Reason}
-spec request(map()) -> {ok, map()} | {error, term()}.
request(_Request) ->
    erlang:nif_error(undefined).

%% @doc Make a GET request.
-spec get(binary()) -> {ok, map()} | {error, term()}.
get(Url) ->
    request(#{method => get, url => Url, headers => [], body => <<>>}).

%% @doc Make a GET request with custom headers.
-spec get(binary(), [{binary(), binary()}]) -> {ok, map()} | {error, term()}.
get(Url, Headers) ->
    request(#{method => get, url => Url, headers => Headers, body => <<>>}).

%% @doc Make a POST request.
-spec post(binary(), [{binary(), binary()}], binary()) -> {ok, map()} | {error, term()}.
post(Url, Headers, Body) ->
    request(#{method => post, url => Url, headers => Headers, body => Body}).

%% @doc Make a PUT request.
-spec put(binary(), [{binary(), binary()}], binary()) -> {ok, map()} | {error, term()}.
put(Url, Headers, Body) ->
    request(#{method => put, url => Url, headers => Headers, body => Body}).

%% @doc Make a DELETE request.
-spec delete(binary()) -> {ok, map()} | {error, term()}.
delete(Url) ->
    request(#{method => delete, url => Url, headers => [], body => <<>>}).

%% @doc Make a DELETE request with custom headers.
-spec delete(binary(), [{binary(), binary()}]) -> {ok, map()} | {error, term()}.
delete(Url, Headers) ->
    request(#{method => delete, url => Url, headers => Headers, body => <<>>}).
