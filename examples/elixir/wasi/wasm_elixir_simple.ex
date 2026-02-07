defmodule WasmElixir do
  @moduledoc "Elixir running on WebAssembly via AtomVM!"

  def start do
    :io.format("~n╔══════════════════════════════════════╗~n")
    :io.format("║     Elixir on WebAssembly (WASI)     ║~n")
    :io.format("║        via AtomVM + wasmtime         ║~n")
    :io.format("╚══════════════════════════════════════╝~n~n")

    demo_lists()
    demo_maps()
    demo_pattern_matching()
    demo_recursion()

    :io.format("~n✨ Demo complete! Elixir runs on WASM! ✨~n~n")
    :ok
  end

  defp demo_lists do
    :io.format("📋 List Operations~n")
    :io.format("───────────────────~n")

    numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    :io.format("Numbers: ~p~n", [numbers])

    doubled = :lists.map(fn x -> x * 2 end, numbers)
    :io.format("Doubled: ~p~n", [doubled])

    evens = :lists.filter(fn x -> rem(x, 2) == 0 end, numbers)
    :io.format("Evens:   ~p~n", [evens])

    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    :io.format("Sum:     ~p~n", [sum])
    :io.format("~n")
  end

  defp demo_maps do
    :io.format("🗺️  Map Operations~n")
    :io.format("───────────────────~n")

    user = %{name: "Alice", role: :developer, level: 42}
    :io.format("User: ~p~n", [user])

    :io.format("Name: ~s, Role: ~p, Level: ~p~n", [
      :maps.get(:name, user),
      :maps.get(:role, user),
      :maps.get(:level, user)
    ])

    :io.format("~n")
  end

  defp demo_pattern_matching do
    :io.format("🎯 Pattern Matching~n")
    :io.format("───────────────────~n")

    process_result({:ok, 42})
    process_result({:ok, "success"})
    process_result({:error, "failed"})
    process_result(:unknown)

    :io.format("~nList matching:~n")
    describe_list([])
    describe_list([1])
    describe_list([1, 2, 3, 4, 5])
    :io.format("~n")
  end

  defp process_result({:ok, value}) do
    :io.format("  ✓ Success with value: ~p~n", [value])
  end

  defp process_result({:error, reason}) do
    :io.format("  ✗ Error: ~s~n", [reason])
  end

  defp process_result(other) do
    :io.format("  ? Unknown result: ~p~n", [other])
  end

  defp describe_list([]) do
    :io.format("  Empty list~n")
  end

  defp describe_list([single]) do
    :io.format("  Single item list: ~p~n", [single])
  end

  defp describe_list([first | rest]) do
    :io.format("  List with head: ~p, tail: ~p~n", [first, rest])
  end

  defp demo_recursion do
    :io.format("🔄 Recursion~n")
    :io.format("───────────────────~n")

    :io.format("Factorials:~n")

    :lists.foreach(
      fn n ->
        :io.format("  ~p! = ~p~n", [n, factorial(n)])
      end,
      [0, 1, 5, 8]
    )

    :io.format("~nFibonacci sequence:~n")

    :lists.foreach(
      fn n ->
        :io.format("  fib(~p) = ~p~n", [n, fibonacci(n)])
      end,
      [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    )

    :io.format("~n")
  end

  defp factorial(0), do: 1
  defp factorial(n) when n > 0, do: n * factorial(n - 1)

  defp fibonacci(0), do: 0
  defp fibonacci(1), do: 1
  defp fibonacci(n) when n > 1, do: fibonacci(n - 1) + fibonacci(n - 2)
end
