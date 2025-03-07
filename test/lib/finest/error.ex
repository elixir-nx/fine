defmodule Finest.Error do
  defexception [:data]

  @impl true
  def message(error) do
    "got error with data #{error.data}"
  end
end
