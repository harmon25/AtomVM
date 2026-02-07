defmodule PureErlangDemo do
  @moduledoc "Elixir demo using only Erlang stdlib - no Elixir stdlib"

  def start do
    # Use Erlang io module instead of Elixir IO
    :io.format("Hello from Elixir on WASI!~n~n")

    # Basic list operations using Erlang lists module
    numbers = [1, 2, 3, 4, 5]
    :io.format("Numbers: ~p~n", [numbers])

    doubled = :lists.map(fn x -> x * 2 end, numbers)
    :io.format("Doubled: ~p~n", [doubled])

    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    :io.format("Sum: ~p~n~n", [sum])

    # Maps - use Erlang maps module
    user = %{name: "Alice", age: 30}
    :io.format("User: ~p~n", [user])

    name = :maps.get(:name, user)
    age = :maps.get(:age, user)
    :io.format("Name: ~s, Age: ~p~n~n", [name, age])

    # Recursion
    result = factorial(5)
    :io.format("5! = ~p~n~n", [result])

    # Binary/string operations using Erlang
    text = "Hello WASI World"
    :io.format("Text: ~s~n", [text])
    :io.format("Length: ~p~n~n", [:string.length(text)])

    # Pattern matching
    :io.format("Pattern matching demo:~n")
    check_value({:ok, 42})
    check_value({:error, "failed"})
    check_value(:unknown)

    :io.format("~nDemo complete!~n")
    :ok
  end

  defp factorial(0), do: 1
  defp factorial(n), do: n * factorial(n - 1)

  defp check_value({:ok, value}) do
    :io.format("  Got ok: ~p~n", [value])
  end

  defp check_value({:error, reason}) do
    :io.format("  Got error: ~s~n", [reason])
  end

  defp check_value(other) do
    :io.format("  Got other: ~p~n", [other])
  end
end
