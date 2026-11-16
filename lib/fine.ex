defmodule Fine do
  @external_resource "README.md"

  [_, readme_docs, _] =
    "README.md"
    |> File.read!()
    |> String.split("<!-- Docs -->")

  @moduledoc readme_docs

  # Note that include/ is a conventional location for Erlang header
  # files (.hrl) and it is copied to _build/. For this reason, we
  # pick a different name.
  #
  # We store the path relative to the deps directory and expand it
  # on call, so that the compiled module does not hold an absolute
  # path of the machine it was compiled on.
  @include_dir Path.relative_to(Path.expand("c_include"), Mix.Project.deps_path())

  @doc """
  Returns the directory with Fine header files.
  """
  @spec include_dir() :: String.t()
  def include_dir(), do: Path.expand(@include_dir, Mix.Project.deps_path())
end
