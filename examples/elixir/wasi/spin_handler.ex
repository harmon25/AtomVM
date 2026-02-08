# SpinHandler - Elixir HTTP handler for AtomVM on Fermyon Spin
#
# This module implements the handler callback for the wasi:http/incoming-handler
# integration. When Spin (or wasmtime serve) receives an HTTP request, AtomVM
# calls SpinHandler.handle/1 with a request map and expects a response map back.
#
# ## Request Map
#
#   %{
#     method:    atom(),           # :get, :post, :put, :delete, :head, :options, :patch, :trace
#     path:      binary(),         # e.g. "/hello?name=world"
#     headers:   [{binary(), binary()}],  # e.g. [{"content-type", "text/plain"}]
#     body:      binary(),         # request body (<<>> if empty)
#     authority: binary()          # host header (optional key)
#   }
#
# ## Response Map
#
#   %{
#     status:  integer(),          # HTTP status code
#     headers: [{binary(), binary()}],
#     body:    binary()
#   }
#
# ## Building
#
#   # Quickest way: use the build script
#   ./build_and_run.sh
#
#   # Or manually:
#
#   # 1. Compile the handler
#   elixirc --no-docs --no-debug-info spin_handler.ex
#
#   # 2. Compile the platform NIF stubs (spin_http, spin_kv, etc.)
#   #    These .beam files must be in the AVM pack for the NIF dispatch to work.
#   erlc src/platforms/wasi/http/spin_http.erl \
#        src/platforms/wasi/http/spin_kv.erl \
#        src/platforms/wasi/http/spin_config.erl \
#        src/platforms/wasi/http/spin_sqlite.erl \
#        src/platforms/wasi/http/spin_postgres.erl
#
#   # 3. Package (handler + NIF stubs + standard libraries)
#   packbeam create app.avm Elixir.SpinHandler.beam \
#       spin_http.beam spin_kv.beam spin_config.beam \
#       spin_sqlite.beam spin_postgres.beam \
#       estdlib.avm eavmlib.avm
#
# ## Running
#
#   # With Spin (see spin.toml):
#   spin up
#   curl http://localhost:3000/

defmodule SpinHandler do
  @moduledoc """
  Elixir HTTP handler for AtomVM running on Fermyon Spin.

  Demonstrates routing, request inspection, JSON responses,
  and request body echo — all in Elixir on WebAssembly.
  """

  @doc """
  Handle an incoming HTTP request.
  """
  def handle(%{method: method, path: path, headers: headers, body: body}) do
    route(method, path, headers, body)
  end

  # -- Routes -----------------------------------------------------------------

  defp route(:get, <<"/">>, _headers, _body) do
    respond(200, "text/html; charset=utf-8", """
    <html>
    <head><title>AtomVM on Spin</title></head>
    <body>
    <h1>Hello from Elixir on AtomVM!</h1>
    <p>Running on Fermyon Spin via WebAssembly.</p>
    <ul>
      <li><a href="/hello">/hello</a> - greeting</li>
      <li><a href="/info">/info</a> - request info</li>
      <li><a href="/json">/json</a> - JSON response</li>
      <li><a href="/compute">/compute</a> - run some computations</li>
      <li><a href="/proxy">/proxy</a> - outbound HTTP (fetch httpbin.org)</li>
      <li><a href="/fetch">/fetch</a> - outbound POST to httpbin echo</li>
      <li>POST <a href="/echo">/echo</a> - echo body back</li>
    </ul>
    </body>
    </html>
    """)
  end

  defp route(:get, <<"/hello">>, _headers, _body) do
    respond(200, "text/plain; charset=utf-8", "Hello from Elixir on AtomVM!")
  end

  defp route(_method, <<"/echo">>, _headers, body) do
    respond(200, "application/octet-stream", body)
  end

  defp route(:get, <<"/info">>, headers, body) do
    header_lines =
      :lists.foldl(
        fn {name, value}, acc ->
          <<acc::binary, "  ", name::binary, ": ", value::binary, "\n">>
        end,
        <<>>,
        headers
      )

    body_info =
      case byte_size(body) do
        0 -> "(empty)"
        n -> <<:erlang.integer_to_binary(n)::binary, " bytes">>
      end

    text = <<
      "Method: GET\n",
      "Headers:\n",
      header_lines::binary,
      "Body: ",
      body_info::binary,
      "\n"
    >>

    respond(200, "text/plain; charset=utf-8", text)
  end

  defp route(:get, <<"/json">>, _headers, _body) do
    # Build a simple JSON response by hand
    # (no JSON library needed)
    json = ~c"""
    {"message":"Hello from Elixir on AtomVM","platform":"wasi","features":["pattern_matching","recursion","binaries","maps"]}
    """

    respond(200, "application/json", :erlang.list_to_binary(json))
  end

  defp route(:get, <<"/compute">>, _headers, _body) do
    # Demonstrate that real Elixir computation works on WASM
    fib_10 = fib(10)
    fact_12 = factorial(12)

    # List operations using Erlang stdlib (works without exavmlib)
    squares = :lists.map(fn x -> x * x end, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10])
    sum = :lists.foldl(fn x, acc -> x + acc end, 0, squares)
    evens = :lists.filter(fn x -> rem(x, 2) == 0 end, [1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

    fib_bin = :erlang.integer_to_binary(fib_10)
    fact_bin = :erlang.integer_to_binary(fact_12)
    sum_bin = :erlang.integer_to_binary(sum)
    evens_bin = format_list(evens)
    squares_bin = format_list(squares)

    text = <<
      "Elixir computations on WebAssembly:\n\n",
      "  fib(10)     = ",
      fib_bin::binary,
      "\n",
      "  factorial(12)= ",
      fact_bin::binary,
      "\n",
      "  squares     = ",
      squares_bin::binary,
      "\n",
      "  sum(squares)= ",
      sum_bin::binary,
      "\n",
      "  evens(1..10)= ",
      evens_bin::binary,
      "\n",
      "\nAll computed in Elixir running on AtomVM/WASM!\n"
    >>

    respond(200, "text/plain; charset=utf-8", text)
  end

  defp route(:get, <<"/proxy">>, _headers, _body) do
    # Demonstrate outbound HTTP: fetch from httpbin
    case :spin_http.get("https://httpbin.org/get") do
      {:ok, %{status: upstream_status, body: upstream_body}} ->
        status_bin = :erlang.integer_to_binary(upstream_status)

        respond(200, "text/plain; charset=utf-8", <<
          "Upstream status: ",
          status_bin::binary,
          "\n\n",
          upstream_body::binary
        >>)

      {:error, reason} ->
        err_bin = :erlang.atom_to_binary(reason, :utf8)
        respond(502, "text/plain", <<"Upstream request failed: ", err_bin::binary>>)
    end
  end

  defp route(:get, <<"/fetch">>, _headers, _body) do
    # POST to httpbin echo
    case :spin_http.post(
           "https://httpbin.org/post",
           [{"content-type", "application/json"}],
           "{\"from\":\"Elixir on AtomVM\",\"runtime\":\"WASM\"}"
         ) do
      {:ok, %{body: resp_body}} ->
        respond(200, "application/json", resp_body)

      {:error, _} ->
        respond(502, "text/plain", "Failed to reach upstream")
    end
  end

  defp route(_method, _path, _headers, _body) do
    respond(404, "text/plain", "Not Found")
  end

  # -- Response helpers -------------------------------------------------------

  defp respond(status, content_type, body) do
    %{
      status: status,
      headers: [{<<"content-type">>, content_type}],
      body: body
    }
  end

  # -- Computation demos ------------------------------------------------------

  defp fib(0), do: 0
  defp fib(1), do: 1
  defp fib(n), do: fib(n - 1) + fib(n - 2)

  defp factorial(0), do: 1
  defp factorial(n) when n > 0, do: n * factorial(n - 1)

  defp format_list(list) do
    inner =
      :lists.foldl(
        fn
          elem, <<>> -> :erlang.integer_to_binary(elem)
          elem, acc -> <<acc::binary, ", ", :erlang.integer_to_binary(elem)::binary>>
        end,
        <<>>,
        list
      )

    <<"[", inner::binary, "]">>
  end
end
