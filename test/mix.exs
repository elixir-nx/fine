defmodule Finest.MixProject do
  use Mix.Project

  def project do
    [
      app: :finest,
      version: "0.1.0",
      elixir: "~> 1.15",
      start_permanent: Mix.env() == :prod,
      compilers: [:elixir_make] ++ Mix.compilers(),
      deps: deps(),
      make_env: fn -> %{"FINE_INCLUDE_DIR" => Fine.include_dir()} end
    ]
  end

  def application do
    []
  end

  defp deps do
    [
      {:fine, path: "..", runtime: false},
      {:elixir_make, "~> 0.9", runtime: false}
    ]
  end
end
