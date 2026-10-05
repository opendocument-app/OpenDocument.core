#include "odr_jni.hpp"

#include <odr/error_code.hpp>
#include <odr/exceptions.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

void append_utf8(std::string &out, const std::uint32_t code_point) {
  if (code_point < 0x80) {
    out.push_back(static_cast<char>(code_point));
  } else if (code_point < 0x800) {
    out.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  } else if (code_point < 0x10000) {
    out.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  } else {
    out.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
    out.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
  }
}

} // namespace

std::string odr_jni::to_string(JNIEnv *env, jstring string) {
  if (string == nullptr) {
    return {};
  }
  const jsize length = env->GetStringLength(string);
  const jchar *chars = env->GetStringChars(string, nullptr);
  if (chars == nullptr) {
    throw std::bad_alloc();
  }
  const auto release = [env, string](const jchar *data) {
    env->ReleaseStringChars(string, data);
  };
  const std::unique_ptr<const jchar, decltype(release)> guard(chars, release);
  std::string result;
  result.reserve(static_cast<std::size_t>(length));
  for (jsize i = 0; i < length; ++i) {
    std::uint32_t code_point = chars[i];
    if (code_point >= 0xd800 && code_point <= 0xdbff && i + 1 < length &&
        chars[i + 1] >= 0xdc00 && chars[i + 1] <= 0xdfff) {
      code_point =
          0x10000 + ((code_point - 0xd800) << 10) + (chars[i + 1] - 0xdc00);
      ++i;
    }
    if (code_point >= 0xd800 && code_point <= 0xdfff) {
      throw std::invalid_argument("unpaired UTF-16 surrogate");
    }
    append_utf8(result, code_point);
  }
  return result;
}

jstring odr_jni::to_jstring(JNIEnv *env, const std::string_view string) {
  std::vector<jchar> units;
  units.reserve(string.size());
  for (std::size_t i = 0; i < string.size();) {
    const auto byte = static_cast<std::uint8_t>(string[i]);
    std::uint32_t code_point = 0xfffd;
    std::size_t sequence_length = 1;
    if (byte < 0x80) {
      code_point = byte;
    } else if (byte >= 0xc2 && byte <= 0xdf) {
      code_point = byte & 0x1f;
      sequence_length = 2;
    } else if ((byte >> 4) == 0xe) {
      code_point = byte & 0x0f;
      sequence_length = 3;
    } else if (byte >= 0xf0 && byte <= 0xf4) {
      code_point = byte & 0x07;
      sequence_length = 4;
    }
    if (sequence_length > string.size() - i) {
      code_point = 0xfffd;
      sequence_length = 1;
    } else {
      for (std::size_t j = 1; j < sequence_length; ++j) {
        const auto continuation = static_cast<std::uint8_t>(string[i + j]);
        if ((continuation >> 6) != 0x2) {
          code_point = 0xfffd;
          sequence_length = 1;
          break;
        }
        code_point = (code_point << 6) | (continuation & 0x3f);
      }
    }
    if ((sequence_length == 2 && code_point < 0x80) ||
        (sequence_length == 3 && code_point < 0x800) ||
        (sequence_length == 4 && code_point < 0x10000) ||
        (code_point >= 0xd800 && code_point <= 0xdfff) ||
        code_point > 0x10ffff) {
      code_point = 0xfffd;
    }
    i += sequence_length;
    if (code_point >= 0x10000) {
      const std::uint32_t offset = code_point - 0x10000;
      units.push_back(static_cast<jchar>(0xd800 + (offset >> 10)));
      units.push_back(static_cast<jchar>(0xdc00 + (offset & 0x3ff)));
    } else {
      units.push_back(static_cast<jchar>(code_point));
    }
  }
  if (units.size() >
      static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
    throw std::length_error("Java string is too long");
  }
  return env->NewString(units.data(), static_cast<jsize>(units.size()));
}

jbyteArray odr_jni::to_jbytes(JNIEnv *env, const std::string_view bytes) {
  if (bytes.size() >
      static_cast<std::size_t>(std::numeric_limits<jsize>::max())) {
    throw std::length_error("Java byte array is too long");
  }
  const auto length = static_cast<jsize>(bytes.size());
  jbyteArray result = env->NewByteArray(length);
  if (result == nullptr) {
    return nullptr;
  }
  env->SetByteArrayRegion(result, 0, length,
                          reinterpret_cast<const jbyte *>(bytes.data()));
  return result;
}

namespace {

/// Returns false if the exception class or constructor is absent.
bool throw_coded(JNIEnv *env, const char *class_name, const char *message,
                 const odr::ErrorCode code) {
  jstring text = odr_jni::to_jstring(env, message);
  if (text == nullptr) {
    return true;
  }
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    env->ExceptionClear();
    env->DeleteLocalRef(text);
    return false;
  }
  const jmethodID constructor =
      env->GetMethodID(cls, "<init>", "(Ljava/lang/String;I)V");
  if (constructor == nullptr) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(text);
    return false;
  }
  auto throwable = static_cast<jthrowable>(
      env->NewObject(cls, constructor, text, static_cast<jint>(code)));
  env->DeleteLocalRef(text);
  env->DeleteLocalRef(cls);
  if (throwable == nullptr) {
    return true; // Preserve the pending allocation exception.
  }
  env->Throw(throwable);
  env->DeleteLocalRef(throwable);
  return true;
}

} // namespace

void odr_jni::throw_java(JNIEnv *env) noexcept {
  if (env->ExceptionCheck()) {
    return;
  }
  try {
    constexpr auto base = "app/opendocument/core/OdrException";
    try {
      throw;
    } catch (const std::exception &e) {
      const odr::ErrorCode code = odr::error_code(e);
      // The nested class is named after the code; one with no class of its own
      // falls back to the base.
      const std::string name =
          std::string(base) + "$" + std::string(odr::error_code_name(code));
      if (code != odr::ErrorCode::unknown &&
          throw_coded(env, name.c_str(), e.what(), code)) {
        return;
      }
      throw_coded(env, base, e.what(), code);
    } catch (...) {
      throw_coded(env, base, "unknown native error", odr::ErrorCode::unknown);
    }
  } catch (...) {
    if (!env->ExceptionCheck()) {
      const jclass cls = env->FindClass("java/lang/OutOfMemoryError");
      if (cls != nullptr) {
        env->ThrowNew(cls, "could not construct native exception");
        env->DeleteLocalRef(cls);
      }
    }
  }
}
