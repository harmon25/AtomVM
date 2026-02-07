-module(elixir_runner).
-export([start/0]).

start() ->
    'Elixir.WasmElixir':start().