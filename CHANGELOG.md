# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [v0.1.1](https://github.com/elixir-nx/fine/tree/v0.1.1) (2025-06-27)

### Added

- Encoding and decoding for `std::string_view` as a better alternative to `ErlNifBinary` ([#4](https://github.com/elixir-nx/fine/pull/4))
- `fine::make_new_binary` to streamline returning large buffers ([#6](https://github.com/elixir-nx/fine/pull/6))
- Erlang-backed synchronization primitives in `fine/sync.hpp` ([#7](https://github.com/elixir-nx/fine/pull/7))

## [v0.1.0](https://github.com/elixir-nx/fine/tree/v0.1.0) (2025-02-19)

Initial release.
