#pragma once

#include <jni.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace odr_jni {

/// Java string (UTF-16) to UTF-8. Handles nullptr and supplementary planes
/// (unlike JNI's modified UTF-8 accessors).
std::string to_string(JNIEnv *env, jstring string);
/// UTF-8 to Java string (UTF-16).
jstring to_jstring(JNIEnv *env, std::string_view string);

jbyteArray to_jbytes(JNIEnv *env, std::string_view bytes);

/// Rethrows the pending C++ exception as the matching Java exception
/// (`app.opendocument.core.OdrException` and subclasses).
void throw_java(JNIEnv *env);

/// Runs `f`, converting any C++ exception into a pending Java exception and
/// returning a value-initialized result (ignored by the JVM in that case).
template <typename F> auto guarded(JNIEnv *env, F &&f) -> decltype(f()) {
  using Result = decltype(f());
  try {
    return std::forward<F>(f)();
  } catch (...) {
    throw_java(env);
  }
  if constexpr (!std::is_void_v<Result>) {
    return Result{};
  }
}

template <typename T> T *from_handle(jlong handle) {
  return reinterpret_cast<T *>(handle);
}

/// Moves `value` to the heap and returns the pointer as a Java handle.
template <typename T> jlong make_handle(T value) {
  return reinterpret_cast<jlong>(new T(std::move(value)));
}

/// Frees a handle. Guarded like every other native body: a destructor that
/// throws would otherwise unwind through the JNI boundary.
template <typename T> void destroy_handle(JNIEnv *env, jlong handle) {
  guarded(env, [&] { delete from_handle<T>(handle); });
}

/// Owns the handles made for one Java allocation until `release` hands them
/// over. Without it a failing `NewLongArray`/`NewObject` strands the C++ copies
/// the handles point at.
template <typename T> class HandleGuard {
public:
  explicit HandleGuard(const std::size_t capacity = 0) {
    m_handles.reserve(capacity);
  }

  HandleGuard(const HandleGuard &) = delete;
  HandleGuard &operator=(const HandleGuard &) = delete;

  ~HandleGuard() {
    for (const jlong handle : m_handles) {
      delete from_handle<T>(handle);
    }
  }

  jlong add(T value) {
    return m_handles.emplace_back(make_handle(std::move(value)));
  }

  [[nodiscard]] const std::vector<jlong> &handles() const { return m_handles; }

  void release() { m_handles.clear(); }

private:
  std::vector<jlong> m_handles;
};

} // namespace odr_jni
