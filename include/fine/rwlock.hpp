#ifndef FINE_RWLOCK_HPP
#define FINE_RWLOCK_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <shared_mutex> // IWYU pragma: keep
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <erl_nif.h>

namespace fine {
// Creates a read-write lock backed by Erlang.
//
// This lock type implements the Lockable and SharedLockable requirements,
// ensuring it can be used with `std::unique_lock`, `std::shared_lock`, etc.
class RwLock final {
public:
  // Creates an unnamed RwLock.
  inline RwLock() noexcept : m_handle(enif_rwlock_create(nullptr)) {
    if (!m_handle) {
      throw std::runtime_error("failed to create rwlock");
    }
  }

  // Creates a RwLock from a ErlNifRWLock handle.
  inline explicit RwLock(ErlNifRWLock *handle) noexcept : m_handle(handle) {}

  // Creates a RwLock with the given name.
  inline RwLock(std::string_view app, std::string_view type,
                std::optional<std::string_view> instance = std::nullopt) {
    std::stringstream stream;
    stream << app << "." << type;

    if (instance) {
      stream << "[" << *instance << "]";
    }

    std::string str = std::move(stream).str();

    // We make use of `const_cast` to create the rwlock, but this exceptional
    // situation is acceptable, as `enif_rwlock_create` doesn't modify the name:
    //   https://github.com/erlang/otp/blob/a87183f1eb847119b6ecc83054bf13c26b8ccfaa/ert/emulator/beam/erl_drv_thread.c#L337-L340
    auto *handle = enif_rwlock_create(const_cast<char *>(str.c_str()));
    if (handle == nullptr) {
      throw std::runtime_error("failed to create rwlock");
    }
    m_handle.reset(handle);
  }

  // Converts this RwLock to a ErlNifRwLock handle.
  //
  // Ownership still belongs to this instance.
  inline operator ErlNifRWLock *() const & noexcept { return m_handle.get(); }

  // Releases ownership of the ErlNifRWLock handle to the caller.
  //
  // This operation is only possible by:
  // ```
  // static_cast<ErlNifRWLock*>(std::move(rwlock))
  // ```
  inline explicit operator ErlNifRWLock *() && noexcept {
    return m_handle.release();
  }

  // Read locks an RwLock. The calling thread is blocked until the RwLock has
  // been read locked. A thread that currently has read or read/write locked the
  // RwLock cannot lock the same RwLock again.
  //
  // This function is thread-safe.
  inline void lock_shared() noexcept { enif_rwlock_rlock(m_handle.get()); }

  // Read unlocks an RwLock. The RwLock currently must be read locked by the
  // calling thread.
  //
  // This function is thread-safe.
  inline void unlock_shared() noexcept { enif_rwlock_runlock(m_handle.get()); }

  // Read/write locks an RwLock. The calling thread is blocked until the RwLock
  // has been read/write locked. A thread that currently has read or read/write
  // locked the RwLock cannot lock the same RwLock again.
  //
  // This function is thread-safe.
  inline void lock() noexcept { enif_rwlock_rwlock(m_handle.get()); }

  // Read/write unlocks an RwLock. The RwLock currently must be read/write
  // locked by the calling thread.
  //
  // This function is thread-safe.
  inline void unlock() noexcept { enif_rwlock_rwunlock(m_handle.get()); }

  // Tries to read lock an RwLock.
  //
  // This function is thread-safe.
  inline bool try_lock_shared() noexcept {
    return enif_rwlock_tryrlock(m_handle.get()) == 0;
  }

  // Tries to read/write lock an RwLock. A thread that currently has read or
  // read/write locked the RwLock cannot try to lock the same RwLock again.
  //
  // This function is thread-safe.
  inline bool try_lock() noexcept {
    return enif_rwlock_tryrwlock(m_handle.get()) == 0;
  }

private:
  struct Deleter {
    inline void operator()(ErlNifRWLock *handle) const noexcept {
      enif_rwlock_destroy(handle);
    }
  };
  std::unique_ptr<ErlNifRWLock, Deleter> m_handle;
};
} // namespace fine

#endif
