#include <fine.hpp>

int64_t add(ErlNifEnv *env, int64_t x, int64_t y) { return x + y; }

FINE_NIF(add, 0);
FINE_INIT("Elixir.Example");
