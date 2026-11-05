#include <atomic>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <thread>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <erl_nif.h>
#include <fine.hpp>
#include <fine/sync.hpp>

namespace finest {

namespace {
#ifdef _WIN32
ErlNifEvent create_test_event() {
  return CreateEventW(nullptr, FALSE, FALSE, nullptr);
}

bool is_valid_test_event(ErlNifEvent event) { return event != nullptr; }

bool close_test_event(ErlNifEvent event) {
  return CloseHandle(event) != FALSE;
}

int64_t test_event_id(ErlNifEvent event) {
  return static_cast<int64_t>(reinterpret_cast<std::intptr_t>(event));
}
#else
ErlNifEvent create_test_event() { return socket(AF_INET, SOCK_DGRAM, 0); }

bool is_valid_test_event(ErlNifEvent event) { return event >= 0; }

bool close_test_event(ErlNifEvent event) { return close(event) == 0; }

int64_t test_event_id(ErlNifEvent event) {
  return static_cast<int64_t>(event);
}
#endif
} // namespace

namespace atoms {
auto ElixirFinestNIF = fine::Atom("Elixir.Finest.NIF");
auto TestResource = fine::Atom("TestResource");
auto ElixirFinestError = fine::Atom("Elixir.Finest.Error");
auto ElixirFinestPoint = fine::Atom("Elixir.Finest.Point");
auto data = fine::Atom("data");
auto destructor_with_env = fine::Atom("destructor_with_env");
auto destructor_default = fine::Atom("destructor_default");
auto error = fine::Atom("error");
auto down = fine::Atom("down");
auto nil = fine::Atom("nil");
auto undefined = fine::Atom("undefined");
auto x = fine::Atom("x");
auto y = fine::Atom("y");
} // namespace atoms

struct TestResource {
  ErlNifPid pid;
  std::string binary;
  using DyncallData = struct {
      int64_t op; // char: '+', '-', '*', '/'
      int64_t x;
      int64_t y;
      int64_t res;
  };

  TestResource(ErlNifPid pid) : pid(pid), binary("hello world") {}

  // {dtor}
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

  // {stop}
  void select_stop(ErlNifEnv* env, ErlNifEvent event, bool direct_call) {
    auto msg_env = enif_alloc_env();
    fine::Term msg;
    if (close_test_event(event)) {
      msg = fine::encode(msg_env, fine::Ok(std::make_tuple(test_event_id(event), direct_call)));
    }
    else {
      msg = fine::encode(msg_env, atoms::error);
    }
    enif_send(env, &this->pid, msg_env, msg);
    enif_free_env(msg_env);
  }

  // {down}
  void monitor_down(ErlNifEnv* env, ErlNifPid mon_pid, const ErlNifMonitor& mon) {
    auto msg_env = enif_alloc_env();
    auto msg = enif_make_tuple3(msg_env,
                                fine::encode(msg_env, atoms::down),
                                enif_make_monitor_term(msg_env, &mon),
                                fine::encode(msg_env, mon_pid));
    enif_send(env, &this->pid, msg_env, msg);
    enif_free_env(msg_env);
  }

  // {dyncall}
  void dynamic_call(ErlNifEnv* /*env*/, void* call_data) {
    DyncallData* data = static_cast<DyncallData*>(call_data);
    switch (data->op) {
    case '+': data->res = data->x + data->y; break;
    case '-': data->res = data->x - data->y; break;
    case '*': data->res = data->x * data->y; break;
    case '/': data->res = data->y ? data->x / data->y : 0; break;
    default:
        data->res = 0;
        break;
    }
  }
};
FINE_RESOURCE(TestResource);
static_assert(fine::ResourcePtr<TestResource>::has_destructor);
static_assert(fine::ResourcePtr<TestResource>::has_select_stop);
static_assert(fine::ResourcePtr<TestResource>::has_monitor_down);
static_assert(fine::ResourcePtr<TestResource>::has_dynamic_call);

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

template <typename T, typename E>
using Result = std::variant<fine::Ok<T>, fine::Error<E>>;

template <typename T> struct FailingAllocator {
  using value_type = T;
  T *allocate(std::size_t) { throw std::bad_alloc(); }
  void deallocate(T *, std::size_t) {}
};

Result<int64_t, std::string> try_decode_int64(ErlNifEnv *env, fine::Term term) {
  auto result = fine::try_decode<const int64_t>(env, term);
  if (!result) {
    return fine::Error(result.error().message(env));
  }
  return fine::Ok(result.value());
}
FINE_NIF(try_decode_int64, 0);

bool try_decode_allocation_error(ErlNifEnv *env, fine::Term term) {
  using Vector = std::vector<int64_t, FailingAllocator<int64_t>>;
  try {
    fine::try_decode<std::variant<Vector, fine::Term>>(env, term);
    return false;
  } catch (const std::bad_alloc &) {
    return true;
  }
}
FINE_NIF(try_decode_allocation_error, 0);

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

std::basic_string<char, std::char_traits<char>,
                  std::pmr::polymorphic_allocator<char>>
codec_string_alloc(ErlNifEnv *,
                   std::basic_string<char, std::char_traits<char>,
                                     std::pmr::polymorphic_allocator<char>>
                       term) {
  return term;
}
FINE_NIF(codec_string_alloc, 0);

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

using TupleVariant = std::variant<std::tuple<int64_t, std::string>,
                                  std::tuple<std::string, int64_t>>;
TupleVariant codec_variant_tuples(ErlNifEnv *, TupleVariant value) {
  return value;
}
FINE_NIF(codec_variant_tuples, 0);

std::tuple<int64_t, std::string>
codec_tuple_int64_and_string(ErlNifEnv *,
                             std::tuple<int64_t, std::string> term) {
  return term;
}
FINE_NIF(codec_tuple_int64_and_string, 0);

Result<std::string, int64_t> codec_result_string_int64_ok(ErlNifEnv *,
                                                          std::string term) {
  return fine::Ok(term);
}
FINE_NIF(codec_result_string_int64_ok, 0);

Result<std::string, int64_t> codec_result_string_int64_error(ErlNifEnv *,
                                                             int64_t term) {
  return fine::Error(term);
}
FINE_NIF(codec_result_string_int64_error, 0);

Result<std::string, int64_t>
codec_result_string_int64_ok_conversion(ErlNifEnv *) {
  return fine::Ok("fine");
}
FINE_NIF(codec_result_string_int64_ok_conversion, 0);

Result<std::string, int64_t>
codec_result_string_int64_error_conversion(ErlNifEnv *) {
  uint16_t result = 42;
  return fine::Error(result);
}
FINE_NIF(codec_result_string_int64_error_conversion, 0);

std::variant<fine::Ok<int64_t, std::string>, fine::Error<>>
codec_result_int64_string_void_ok_conversion(ErlNifEnv *) {
  return fine::Ok(static_cast<int32_t>(201702), "c++17");
}
FINE_NIF(codec_result_int64_string_void_ok_conversion, 0);

std::vector<int64_t> codec_vector_int64(ErlNifEnv *,
                                        std::vector<int64_t> term) {
  return term;
}
FINE_NIF(codec_vector_int64, 0);

std::vector<int64_t, std::pmr::polymorphic_allocator<int64_t>>
codec_vector_int64_alloc(
    ErlNifEnv *,
    std::vector<int64_t, std::pmr::polymorphic_allocator<int64_t>> term) {
  return term;
}
FINE_NIF(codec_vector_int64_alloc, 0);

std::map<fine::Atom, int64_t>
codec_map_atom_int64(ErlNifEnv *, std::map<fine::Atom, int64_t> term) {
  return term;
}
FINE_NIF(codec_map_atom_int64, 0);

std::map<fine::Atom, int64_t, std::less<fine::Atom>,
         std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
codec_map_atom_int64_alloc(
    ErlNifEnv *,
    std::map<
        fine::Atom, int64_t, std::less<fine::Atom>,
        std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
        term) {
  return term;
}
FINE_NIF(codec_map_atom_int64_alloc, 0);

std::unordered_map<fine::Atom, int64_t>
codec_unordered_map_atom_int64(ErlNifEnv *,
                               std::unordered_map<fine::Atom, int64_t> term) {
  return term;
}
FINE_NIF(codec_unordered_map_atom_int64, 0);

std::unordered_map<
    fine::Atom, int64_t, std::hash<fine::Atom>, std::equal_to<fine::Atom>,
    std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
codec_unordered_map_atom_int64_alloc(
    ErlNifEnv *,
    std::unordered_map<
        fine::Atom, int64_t, std::hash<fine::Atom>, std::equal_to<fine::Atom>,
        std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
        term) {
  return term;
}
FINE_NIF(codec_unordered_map_atom_int64_alloc, 0);

std::multimap<fine::Atom, int64_t>
codec_multimap_atom_int64(ErlNifEnv *,
                          std::multimap<fine::Atom, int64_t> term) {
  return term;
}
FINE_NIF(codec_multimap_atom_int64, 0);

std::multimap<
    fine::Atom, int64_t, std::less<fine::Atom>,
    std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
codec_multimap_atom_int64_alloc(
    ErlNifEnv *,
    std::multimap<
        fine::Atom, int64_t, std::less<fine::Atom>,
        std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
        term) {
  return term;
}
FINE_NIF(codec_multimap_atom_int64_alloc, 0);

std::unordered_multimap<fine::Atom, int64_t>
codec_unordered_multimap_atom_int64(
    ErlNifEnv *, std::unordered_multimap<fine::Atom, int64_t> term) {
  return term;
}
FINE_NIF(codec_unordered_multimap_atom_int64, 0);

std::unordered_multimap<
    fine::Atom, int64_t, std::hash<fine::Atom>, std::equal_to<fine::Atom>,
    std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
codec_unordered_multimap_atom_int64_alloc(
    ErlNifEnv *,
    std::unordered_multimap<
        fine::Atom, int64_t, std::hash<fine::Atom>, std::equal_to<fine::Atom>,
        std::pmr::polymorphic_allocator<std::pair<const fine::Atom, int64_t>>>
        term) {
  return term;
}
FINE_NIF(codec_unordered_multimap_atom_int64_alloc, 0);

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

fine::Term resource_monitor_process(ErlNifEnv* env, fine::ResourcePtr<TestResource> obj, ErlNifPid mon_pid) {
  ErlNifMonitor mon;
  int error = enif_monitor_process(env, obj.get(), &mon_pid, &mon);
  if (error) {
    return fine::encode(env, atoms::nil);
  } else {
    return enif_make_monitor_term(env, &mon);
  }
}
FINE_NIF(resource_monitor_process, 0);

fine::Term resource_select_stop(ErlNifEnv* env, fine::ResourcePtr<TestResource> obj) {
  ErlNifEvent event = create_test_event();
  if (!is_valid_test_event(event)) {
      return fine::encode(env, fine::Error(std::string_view{"create_test_event"}));
  }
  int rc = enif_select(env,
                       event,
                       // **BEWARE**
                       // ERL_NIF_SELECT_STOP operation requires a registered stop callback.
                       // OTP’s implementation only verifies this with:
                       //   ASSERT(resource->type->fn.stop);
                       // In a release BEAM, that assertion is compiled out.
                       // OTP subsequently invokes the null callback,
                       // causing the segmentation fault rather than returning an error.
                       // This is an API precondition violation inside native code,
                       // so BEAM cannot safely turn it into an Elixir exception.
                       ERL_NIF_SELECT_STOP,
                       obj.get(),
                       &obj->pid,
                       fine::encode(env, atoms::undefined)
                      );
  if (rc < 0) {
    return fine::encode(env, fine::Error(std::make_tuple(std::string_view("enif_select"), int64_t(rc))));
  }
  return fine::encode(env, fine::Ok(test_event_id(event)));
}
FINE_NIF(resource_select_stop, 0);

fine::Term resource_dyncall_op(ErlNifEnv* env, fine::Term resource, int64_t op, int64_t x, int64_t y) {
  auto data = TestResource::DyncallData{op, x, y, 0};
  int rc = enif_dynamic_resource_call(env,
                                      fine::encode(env, atoms::ElixirFinestNIF),
                                      fine::encode(env, atoms::TestResource),
                                      resource,
                                      &data
                                     );
  if (rc == 0) {
    return fine::encode(env, fine::Ok(data.res));
  }
  else {
    return fine::encode(env, atoms::error);
  }
}
FINE_NIF(resource_dyncall_op, 0);

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

std::nullopt_t mutex_unique_lock_test(ErlNifEnv *) {
  fine::Mutex mutex;
  mutex.lock();

  std::thread thread([&mutex] { auto lock = std::unique_lock(mutex); });

  mutex.unlock();
  thread.join();

  return std::nullopt;
}
FINE_NIF(mutex_unique_lock_test, 0);

std::nullopt_t mutex_scoped_lock_test(ErlNifEnv *) {
  fine::Mutex mutex1;
  fine::Mutex mutex2("finest", "mutex_scoped_lock_test", "mutex2");

  mutex1.lock();
  mutex2.lock();

  std::thread thread(
      [&mutex1, &mutex2] { auto lock = std::scoped_lock(mutex1, mutex2); });

  mutex2.unlock();
  mutex1.unlock();
  thread.join();

  return std::nullopt;
}
FINE_NIF(mutex_scoped_lock_test, 0);

std::nullopt_t shared_mutex_unique_lock_test(ErlNifEnv *) {
  fine::SharedMutex mutex;

  mutex.lock_shared();

  std::thread thread([&mutex] { auto lock = std::unique_lock(mutex); });

  mutex.unlock_shared();
  thread.join();

  return std::nullopt;
}
FINE_NIF(shared_mutex_unique_lock_test, 0);

std::nullopt_t shared_mutex_shared_lock_test(ErlNifEnv *) {
  fine::SharedMutex mutex("finest", "shared_mutex_shared_lock_test", "mutex");

  mutex.lock();

  std::thread thread([&mutex] { auto lock = std::shared_lock(mutex); });

  mutex.unlock();
  thread.join();

  return std::nullopt;
}
FINE_NIF(shared_mutex_shared_lock_test, 0);

class ResetEvent {
public:
  explicit ResetEvent(const bool signaled = false) noexcept
      : m_signaled{signaled} {}

  void set() {
    {
      auto lock = std::unique_lock{m_mutex};
      m_signaled = true;
    }

    m_cond.notify_one();
  }

  void reset() {
    auto lock = std::unique_lock{m_mutex};
    m_signaled = false;
  }

  void wait() {
    auto lock = std::unique_lock{m_mutex};
    m_cond.wait(lock, [&] { return m_signaled; });
    m_signaled = false;
  }

private:
  bool m_signaled;
  fine::Mutex m_mutex;
  fine::ConditionVariable m_cond;
};

std::nullopt_t condition_variable_test(ErlNifEnv *) {
  ResetEvent event{true};
  event.reset();

  std::thread wait_thread_1([&] {
    event.wait();
    event.set();
  });
  std::thread wait_thread_2([&] {
    event.wait();
    event.set();
  });
  std::thread wait_thread_3([&] {
    event.wait();
    event.set();
  });

  event.set();

  wait_thread_1.join();
  wait_thread_2.join();
  wait_thread_3.join();

  return std::nullopt;
}
FINE_NIF(condition_variable_test, 0);

bool compare_eq(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs == rhs;
}
FINE_NIF(compare_eq, 0);

bool compare_ne(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs != rhs;
}
FINE_NIF(compare_ne, 0);

bool compare_lt(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs < rhs;
}
FINE_NIF(compare_lt, 0);

bool compare_le(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs <= rhs;
}
FINE_NIF(compare_le, 0);

bool compare_gt(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs > rhs;
}
FINE_NIF(compare_gt, 0);

bool compare_ge(ErlNifEnv *, fine::Term lhs, fine::Term rhs) noexcept {
  return lhs >= rhs;
}
FINE_NIF(compare_ge, 0);

std::uint64_t hash_term(ErlNifEnv *, fine::Term term) noexcept {
  return std::invoke(std::hash<fine::Term>{}, term);
}
FINE_NIF(hash_term, 0);

std::uint64_t hash_atom(ErlNifEnv *, fine::Atom atom) noexcept {
  return std::invoke(std::hash<fine::Atom>{}, atom);
}
FINE_NIF(hash_atom, 0);

static bool s_loaded = false;

static auto load_registration = fine::Registration::register_load(
    [](ErlNifEnv *, void **, ERL_NIF_TERM) { s_loaded = true; });

static auto unload_registration = fine::Registration::register_unload(
    [](ErlNifEnv *, void *) noexcept { s_loaded = false; });

bool is_loaded(ErlNifEnv *) { return s_loaded; }
FINE_NIF(is_loaded, 0);

std::string format_term(ErlNifEnv *env, fine::Term term) {
  return fine::format_term(env, term);
}
FINE_NIF(format_term, 0);

std::string format_term_with_limit(ErlNifEnv *env, fine::Term term,
                                   uint64_t limit) {
  return fine::format_term(env, term, limit);
}
FINE_NIF(format_term_with_limit, 0);
} // namespace finest

FINE_INIT("Elixir.Finest.NIF");
