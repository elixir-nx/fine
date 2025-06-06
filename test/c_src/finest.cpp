#include <cstring>
#include <erl_nif.h>
#include <exception>
#include <fine.hpp>
#include <optional>
#include <stdexcept>
#include <thread>

namespace finest {

namespace atoms {
auto ElixirFinestError = fine::Atom("Elixir.Finest.Error");
auto ElixirFinestPoint = fine::Atom("Elixir.Finest.Point");
auto data = fine::Atom("data");
auto destructor_with_env = fine::Atom("destructor_with_env");
auto destructor_default = fine::Atom("destructor_default");
auto x = fine::Atom("x");
auto y = fine::Atom("y");
} // namespace atoms

struct TestResource {
  ErlNifPid pid;
  std::string binary;

  TestResource(ErlNifPid pid) : pid(pid), binary("hello world") {}

  void destructor(ErlNifEnv *env) {
    auto msg_env = enif_alloc_env();
    auto msg = fine::encode(msg_env, atoms::destructor_with_env);
    enif_send(env, &this->pid, msg_env, msg);
    enif_free_env(msg_env);
  }

  ~TestResource() {
    auto target_pid = this->pid;

    // We don't have access to env, so we spawn another thread and
    // pass NULL as the env. In usual cases messages should be sent
    // as part of the custom destructor, as we do above, but here we
    // want to test that both of them are called.
    auto thread = std::thread([target_pid] {
      auto msg_env = enif_alloc_env();
      auto msg = fine::encode(msg_env, atoms::destructor_default);
      enif_send(NULL, &target_pid, msg_env, msg);
      enif_free_env(msg_env);
    });

    thread.detach();
  }
};
FINE_RESOURCE(TestResource);

struct ExPoint {
  int64_t x;
  int64_t y;

  static constexpr auto module = &atoms::ElixirFinestPoint;

  static constexpr auto fields() {
    return std::make_tuple(std::make_tuple(&ExPoint::x, &atoms::x),
                           std::make_tuple(&ExPoint::y, &atoms::y));
  }
};

struct ExError {
  int64_t data;

  static constexpr auto module = &atoms::ElixirFinestError;

  static constexpr auto fields() {
    return std::make_tuple(std::make_tuple(&ExError::data, &atoms::data));
  }

  static constexpr auto is_exception = true;
};

int64_t add(ErlNifEnv *, int64_t x, int64_t y) { return x + y; }
FINE_NIF(add, 0);

fine::Term codec_term(ErlNifEnv *, fine::Term term) { return term; }
FINE_NIF(codec_term, 0);

int64_t codec_int64(ErlNifEnv *, int64_t term) { return term; }
FINE_NIF(codec_int64, 0);

uint64_t codec_uint64(ErlNifEnv *, uint64_t term) { return term; }
FINE_NIF(codec_uint64, 0);

double codec_double(ErlNifEnv *, double term) { return term; }
FINE_NIF(codec_double, 0);

bool codec_bool(ErlNifEnv *, bool term) { return term; }
FINE_NIF(codec_bool, 0);

ErlNifPid codec_pid(ErlNifEnv *, ErlNifPid term) { return term; }
FINE_NIF(codec_pid, 0);

ErlNifBinary codec_binary(ErlNifEnv *, ErlNifBinary term) {
  ErlNifBinary copy;
  enif_alloc_binary(term.size, &copy);
  std::memcpy(copy.data, term.data, term.size);
  return copy;
}
FINE_NIF(codec_binary, 0);

std::string_view codec_string_view(ErlNifEnv *, std::string_view term) {
  return term;
}
FINE_NIF(codec_string_view, 0);

std::string codec_string(ErlNifEnv *, std::string term) { return term; }
FINE_NIF(codec_string, 0);

fine::Atom codec_atom(ErlNifEnv *, fine::Atom term) { return term; }
FINE_NIF(codec_atom, 0);

std::nullopt_t codec_nullopt(ErlNifEnv *) { return std::nullopt; }
FINE_NIF(codec_nullopt, 0);

std::optional<int64_t> codec_optional_int64(ErlNifEnv *,
                                            std::optional<int64_t> term) {
  return term;
}
FINE_NIF(codec_optional_int64, 0);

std::variant<int64_t, std::string>
codec_variant_int64_or_string(ErlNifEnv *,
                              std::variant<int64_t, std::string> term) {
  return term;
}
FINE_NIF(codec_variant_int64_or_string, 0);

std::tuple<int64_t, std::string>
codec_tuple_int64_and_string(ErlNifEnv *,
                             std::tuple<int64_t, std::string> term) {
  return term;
}
FINE_NIF(codec_tuple_int64_and_string, 0);

std::vector<int64_t> codec_vector_int64(ErlNifEnv *,
                                        std::vector<int64_t> term) {
  return term;
}
FINE_NIF(codec_vector_int64, 0);

std::map<fine::Atom, int64_t>
codec_map_atom_int64(ErlNifEnv *, std::map<fine::Atom, int64_t> term) {
  return term;
}
FINE_NIF(codec_map_atom_int64, 0);

fine::ResourcePtr<TestResource>
codec_resource(ErlNifEnv *, fine::ResourcePtr<TestResource> term) {
  return term;
}
FINE_NIF(codec_resource, 0);

ExPoint codec_struct(ErlNifEnv *, ExPoint term) { return term; }
FINE_NIF(codec_struct, 0);

ExError codec_struct_exception(ErlNifEnv *, ExError term) { return term; }
FINE_NIF(codec_struct_exception, 0);

fine::Ok<> codec_ok_empty(ErlNifEnv *) { return fine::Ok(); }
FINE_NIF(codec_ok_empty, 0);

fine::Ok<int64_t> codec_ok_int64(ErlNifEnv *, int64_t term) {
  return fine::Ok(term);
}
FINE_NIF(codec_ok_int64, 0);

fine::Error<> codec_error_empty(ErlNifEnv *) { return fine::Error(); }
FINE_NIF(codec_error_empty, 0);

fine::Error<std::string> codec_error_string(ErlNifEnv *, std::string term) {
  return fine::Error(term);
}
FINE_NIF(codec_error_string, 0);

fine::ResourcePtr<TestResource> resource_create(ErlNifEnv *, ErlNifPid pid) {
  return fine::make_resource<TestResource>(pid);
}
FINE_NIF(resource_create, 0);

ErlNifPid resource_get(ErlNifEnv *, fine::ResourcePtr<TestResource> resource) {
  return resource->pid;
}
FINE_NIF(resource_get, 0);

fine::Term resource_binary(ErlNifEnv *env,
                           fine::ResourcePtr<TestResource> resource) {
  return fine::make_resource_binary(env, resource, resource->binary.data(),
                                    resource->binary.size());
}
FINE_NIF(resource_binary, 0);

fine::Term make_new_binary(ErlNifEnv *env) {
  const char *buffer = "hello world";
  size_t size = 11;
  return fine::make_new_binary(env, buffer, size);
}
FINE_NIF(make_new_binary, 0);

int64_t throw_runtime_error(ErlNifEnv *) {
  throw std::runtime_error("runtime error reason");
}
FINE_NIF(throw_runtime_error, 0);

int64_t throw_invalid_argument(ErlNifEnv *) {
  throw std::invalid_argument("invalid argument reason");
}
FINE_NIF(throw_invalid_argument, 0);

int64_t throw_other_exception(ErlNifEnv *) { throw std::exception(); }
FINE_NIF(throw_other_exception, 0);

int64_t raise_elixir_exception(ErlNifEnv *env) {
  fine::raise(env, ExError{10});

  // MSVC detects that raise throws and treats return as unreachable
#if !defined(_WIN32)
  return 0;
#endif
}
FINE_NIF(raise_elixir_exception, 0);

int64_t raise_erlang_error(ErlNifEnv *env) {
  fine::raise(env, fine::Atom("oops"));

  // MSVC detects that raise throws and treats return as unreachable
#if !defined(_WIN32)
  return 0;
#endif
}
FINE_NIF(raise_erlang_error, 0);

} // namespace finest

FINE_INIT("Elixir.Finest.NIF");
