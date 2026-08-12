defmodule WasmElixir do
  @moduledoc """
  Elixir on WebAssembly via AtomVM!

  This demonstrates core Elixir features running in wasmtime:
  - Pattern matching
  - Immutability
  - Higher-order functions
  - Recursion
  - Pipe operator
  """

  def start do
    print_header()

    demo_lists()
    demo_maps()
    demo_pattern_matching()
    demo_recursion()
    demo_pipes()

    print_footer()
    :ok
  end

  defp print_header do
    :io.format("~n")
    :io.format("╔══════════════════════════════════════╗~n")
    :io.format("║     Elixir on WebAssembly (WASI)     ║~n")
    :io.format("║        via AtomVM + wasmtime         ║~n")
    :io.format("╚══════════════════════════════════════╝~n")
    :io.format("~n")
  end

  defp print_footer do
    :io.format("~n")
    :io.format("✨ Demo complete! Elixir runs on WASM! ✨~n")
    :io.format("~n")
  end

  defp demo_lists do
    :io.format("📋 List Operations~n")
    :io.format("───────────────────~n")

    numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    :io.format("Original: ~p~n", [numbers])

    # Map
    doubled = :lists.map(fn x -> x * 2 end, numbers)
    :io.format("Doubled:  ~p~n", [doubled])

    # Filter
    evens = :lists.filter(fn x -> rem(x, 2) == 0 end, numbers)
    :io.format("Evens:    ~p~n", [evens])

    # Fold (reduce)
    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    :io.format("Sum:      ~p~n", [sum])

    # List comprehension (via filter + map)
    squares_of_odds =
      numbers
      |> :lists.filter(fn x -> rem(x, 2) == 1 end)
      |> :lists.map(fn x -> x * x end)

    :io.format("Odd squares: ~p~n", [squares_of_odds])
    :io.format("~n")
  end

  defp demo_maps do
    :io.format("🗺️  Map Operations~n")
    :io.format("───────────────────~n")

    user = %{
      name: "Alice",
      role: :developer,
      skills: ["Elixir", "Erlang", "WebAssembly"]
    }

    :io.format("User map: ~p~n", [user])
    :io.format("Name: ~s~n", [:maps.get(:name, user)])
    :io.format("Role: ~p~n", [:maps.get(:role, user)])
    :io.format("~n")
  end

  defp demo_pattern_matching do
    :io.format("🎯 Pattern Matching~n")
    :io.format("───────────────────~n")

    # Pattern match on tuples
    process_result({:ok, 42})
    process_result({:ok, "success"})
    process_result({:error, "connection failed"})
    process_result(:unexpected)
    :io.format("~n")

    # Pattern match on lists
    :io.format("List matching:~n")
    describe_list([])
    describe_list([1])
    describe_list([1, 2, 3])
    :io.format("~n")
  end

  defp process_result({:ok, value}) do
    :io.format("  ✓ Success: ~p~n", [value])
  end

  defp process_result({:error, reason}) do
    :io.format("  ✗ Error: ~s~n", [reason])
  end

  defp process_result(other) do
    :io.format("  ? Unknown: ~p~n", [other])
  end

  defp describe_list([]) do
    :io.format("  Empty list~n")
  end

  defp describe_list([single]) do
    :io.format("  Single item: ~p~n", [single])
  end

  defp describe_list([first, second | rest]) do
    :io.format("  First: ~p, Second: ~p, Rest: ~p~n", [first, second, rest])
  end

  defp demo_recursion do
    :io.format("🔄 Recursion~n")
    :io.format("───────────────────~n")

    :io.format("Factorial:~n")

    :lists.foreach(
      fn n ->
        result = factorial(n)
        :io.format("  ~p! = ~p~n", [n, result])
      end,
      [0, 1, 5, 10]
    )

    :io.format("~n")

    :io.format("Fibonacci:~n")
    fibs = :lists.map(fn n -> fibonacci(n) end, [0, 1, 2, 3, 4, 5, 6, 7, 8])
    :io.format("  ~p~n", [fibs])
    :io.format("~n")
  end

  defp factorial(0), do: 1
  defp factorial(n) when n > 0, do: n * factorial(n - 1)

  defp fibonacci(0), do: 0
  defp fibonacci(1), do: 1
  defp fibonacci(n) when n > 1, do: fibonacci(n - 1) + fibonacci(n - 2)

  defp demo_pipes do
    :io.format("🔧 Pipe Operator~n")
    :io.format("───────────────────~n")

    # Demonstrate the pipe operator |> 
    # (simulated via manual nesting since |> is macro-based)

    data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

    # Without pipes (nested):
    result1 =
      :lists.sum(:lists.map(fn x -> x * x end, :lists.filter(fn x -> rem(x, 2) == 0 end, data)))

    :io.format("Without pipes: ~p~n", [result1])

    # With pipes (using intermediate variables):
    result2 =
      data
      |> (fn nums -> :lists.filter(fn x -> rem(x, 2) == 0 end, nums) end).()
      |> (fn nums -> :lists.map(fn x -> x * x end, nums) end).()
      |> (fn nums -> :lists.sum(nums) end).()

    :io.format("With pipes:    ~p~n", [result2])
    :io.format("(Sum of squares of even numbers 1-10)~n")
    :io.format("~n")
  end
end
