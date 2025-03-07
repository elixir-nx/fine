defmodule Example do
  @on_load :__on_load__

  def __on_load__ do
    path = :filename.join(:code.priv_dir(:example), ~c"libexample")

    case :erlang.load_nif(path, 0) do
      :ok -> :ok
      {:error, reason} -> raise "failed to load NIF library, reason: #{inspect(reason)}"
    end
  end

  @doc """
  Adds two numbers using NIF.

  ## Examples

      iex> Example.add(1, 2)
      3
  """
  def add(_x, _y) do
    :erlang.nif_error("nif not loaded")
  end
end
