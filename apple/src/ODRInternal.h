#pragma once

#import <Foundation/Foundation.h>

#include <exception>
#include <string>
#include <string_view>
#include <type_traits>

/// Shared internals of the bindings. Not part of the framework's public
/// headers — nothing here ends up in `Headers/`.

/// The ObjC enums are the C++ enums renumbered by hand, so drift between them
/// is a silent, total misinterpretation of every value that crosses. Assert
/// each pairing next to the code that relies on it.
#define ODR_SAME_ENUM(objc, cxx)                                               \
  static_assert(static_cast<int>(objc) == static_cast<int>(cxx),               \
                #objc " drifted from " #cxx)

NS_ASSUME_NONNULL_BEGIN

namespace odr::apple {

/// Real UTF-8 <-> UTF-16, the way `odr_jni::to_string` does it. Never route
/// strings through `-UTF8String` into a `char *` that outlives the autorelease
/// pool.
std::string to_string(NSString *string);
NSString *to_nsstring(const std::string &string);
NSString *to_nsstring(std::string_view string);

/// Drains `stream` into an `NSData`. The stream APIs of odrcore hand out a
/// `std::istream`; ObjC callers want bytes.
NSData *to_nsdata(std::istream &stream);

/// Fills `*error` from the exception currently being handled. Call only from
/// inside a `catch`.
void fill_error(NSError *_Nullable *_Nullable error);

/// Reports an exception that could not be handed to the caller. Call only from
/// inside a `catch`.
void report_swallowed(const char *what);

/// Runs `body` where the caller has no way to receive an error — an ObjC
/// property, or a `void` method — and returns `fallback` if it throws. Pick a
/// fallback that keeps the caller sane: `YES` for a walker's `end`, so a
/// `while (!end)` loop terminates rather than spins.
template <typename Body>
auto guarded_value(Body &&body, std::invoke_result_t<Body> fallback)
    -> std::invoke_result_t<Body> {
  try {
    return body();
  } catch (const std::exception &e) {
    report_swallowed(e.what());
    return fallback;
  } catch (...) {
    report_swallowed("unknown error");
    return fallback;
  }
}

/// The `void` case of `guarded_value`.
template <typename Body> void guarded_void(Body &&body) {
  try {
    body();
  } catch (const std::exception &e) {
    report_swallowed(e.what());
  } catch (...) {
    report_swallowed("unknown error");
  }
}

/// Runs `body`, mapping any C++ exception onto `*error`. A failed call returns
/// a value-initialised `Result` — `nil`, `NO`, `0` — which is the ObjC
/// convention for "consult the error".
template <typename Body>
auto guarded(NSError *_Nullable *_Nullable error,
             Body &&body) -> decltype(body()) {
  using Result = decltype(body());
  try {
    if constexpr (std::is_void_v<Result>) {
      body();
      return;
    } else {
      return body();
    }
  } catch (...) {
    fill_error(error);
    if constexpr (!std::is_void_v<Result>) {
      return Result{};
    }
  }
}

} // namespace odr::apple

NS_ASSUME_NONNULL_END
