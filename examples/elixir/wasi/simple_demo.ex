defmodule SimpleDemo do
  @moduledoc "Simple Elixir demo for AtomVM on WASI"

  def start do
    IO.puts("Hello from Elixir on WASI!")
    IO.puts("")

    # Basic list operations
    numbers = [1, 2, 3, 4, 5]
    IO.puts("Numbers: #{inspect(numbers)}")

    doubled = Enum.map(numbers, fn x -> x * 2 end)
    IO.puts("Doubled: #{inspect(doubled)}")

    sum = :lists.foldl(fn x, acc -> x + acc end, 0, numbers)
    IO.puts("Sum: #{sum}")
    IO.puts("")

    # Pattern matching with maps
    user = %{name: "Alice", age: 30}
    IO.puts("User: #{inspect(user)}")
    IO.puts("Name: #{user.name}, Age: #{user.age}")
    IO.puts("")

    # Recursion
    result = factorial(5)
    IO.puts("5! = #{result}")
    IO.puts("")

    # Chained operations (note: cannot pipe into :lists.foldl due to argument order)
    evens = Enum.filter([1, 2, 3, 4, 5], fn x -> rem(x, 2) == 0 end)
    squares = Enum.map(evens, fn x -> x * x end)
    result2 = :lists.foldl(fn x, acc -> x + acc end, 0, squares)

    IO.puts("Sum of squares of evens: #{result2}")
    IO.puts("")

    IO.puts("Demo complete!")
    :ok
  end

  defp factorial(0), do: 1
  defp factorial(n), do: n * factorial(n - 1)
end
