defmodule Finest.NIF do
  @moduledoc false

  @on_load :__on_load__

  def __on_load__ do
    path = :filename.join(:code.priv_dir(:finest), ~c"libfinest")

    case :erlang.load_nif(path, 0) do
      :ok -> :ok
      {:error, reason} -> raise "failed to load NIF library, reason: #{inspect(reason)}"
    end
  end

  def add(_x, _y), do: err!()

  def codec_term(_term), do: err!()
  def codec_int64(_term), do: err!()
  def codec_uint64(_term), do: err!()
  def codec_double(_term), do: err!()
  def codec_bool(_term), do: err!()
  def codec_pid(_term), do: err!()
  def codec_binary(_term), do: err!()
  def codec_string_view(_term), do: err!()
  def codec_string(_term), do: err!()
  def codec_atom(_term), do: err!()
  def codec_nullopt(), do: err!()
  def codec_optional_int64(_term), do: err!()
  def codec_variant_int64_or_string(_term), do: err!()
  def codec_tuple_int64_and_string(_term), do: err!()
  def codec_vector_int64(_term), do: err!()
  def codec_map_atom_int64(_term), do: err!()
  def codec_resource(_term), do: err!()
  def codec_struct(_term), do: err!()
  def codec_struct_exception(_term), do: err!()
  def codec_ok_empty(), do: err!()
  def codec_ok_int64(_term), do: err!()
  def codec_error_empty(), do: err!()
  def codec_error_string(_term), do: err!()

  def resource_create(_pid), do: err!()
  def resource_get(_resource), do: err!()

  def make_new_binary(), do: err!()

  def throw_runtime_error(), do: err!()
  def throw_invalid_argument(), do: err!()
  def throw_other_exception(), do: err!()
  def raise_elixir_exception(), do: err!()
  def raise_erlang_error(), do: err!()

  defp err!(), do: :erlang.nif_error(:not_loaded)
end
