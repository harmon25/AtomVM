defmodule WasmDemo do
  @moduledoc """
  A simple Elixir demo for AtomVM on WASI.
  """

  def main do
    IO.puts("Hello from Elixir on WASI!")
    IO.puts("")

    # List operations
    numbers = [1, 2, 3, 4, 5]
    IO.puts("Original list: #{inspect(numbers)}")

    doubled = Enum.map(numbers, &(&1 * 2))
    IO.puts("Doubled: #{inspect(doubled)}")

    sum = Enum.sum(numbers)
    IO.puts("Sum: #{sum}")

    evens = Enum.filter(numbers, &(rem(&1, 2) == 0))
    IO.puts("Evens: #{inspect(evens)}")
    IO.puts("")

    # Pattern matching
    user = %{name: "Alice", age: 30, role: :admin}
    IO.puts("User: #{inspect(user)}")

    case user do
      %{role: :admin} ->
        IO.puts("#{user.name} is an admin")

      %{role: :user} ->
        IO.puts("#{user.name} is a regular user")

      _ ->
        IO.puts("Unknown role")
    end

    IO.puts("")

    # Recursion
    IO.puts("Factorial of 5: #{factorial(5)}")
    IO.puts("")

    # String manipulation
    message = "  Hello WASI World  "
    IO.puts("Original: '#{message}'")
    IO.puts("Trimmed: '#{String.trim(message)}'")
    IO.puts("Uppercase: '#{String.upcase(String.trim(message))}'")
    IO.puts("Words: #{inspect(String.split(String.trim(message)))}")
    IO.puts("")

    # Pipe operator
    result =
      1..10
      |> Enum.filter(&(rem(&1, 2) == 0))
      |> Enum.map(&(&1 * &1))
      |> Enum.sum()

    IO.puts("Sum of squares of even numbers 1-10: #{result}")
    IO.puts("")

    IO.puts("Demo complete!")
    :ok
  end

  defp factorial(0), do: 1

  defp factorial(n) when n > 0 do
    n * factorial(n - 1)
  end
end

# Start the demo
WasmDemo.main()
