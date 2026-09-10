#include "odr_jni.hpp"

#include <odr/error_code.hpp>
#include <odr/exceptions.hpp>

#include <cstdint>
#include <vector>

namespace odr_jni {

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

std::string to_string(JNIEnv *env, jstring string) {
  if (string == nullptr) {
    return {};
  }
  const jsize length = env->GetStringLength(string);
  const jchar *chars = env->GetStringChars(string, nullptr);
  if (chars == nullptr) {
    return {};
  }
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
    append_utf8(result, code_point);
  }
  env->ReleaseStringChars(string, chars);
  return result;
}

jstring to_jstring(JNIEnv *env, const std::string_view string) {
  std::vector<jchar> units;
  units.reserve(string.size());
  for (std::size_t i = 0; i < string.size();) {
    const auto byte = static_cast<unsigned char>(string[i]);
    std::uint32_t code_point = 0xfffd;
    std::size_t sequence_length = 1;
    if (byte < 0x80) {
      code_point = byte;
    } else if ((byte >> 5) == 0x6) {
      code_point = byte & 0x1f;
      sequence_length = 2;
    } else if ((byte >> 4) == 0xe) {
      code_point = byte & 0x0f;
      sequence_length = 3;
    } else if ((byte >> 3) == 0x1e) {
      code_point = byte & 0x07;
      sequence_length = 4;
    }
    if (i + sequence_length > string.size()) {
      code_point = 0xfffd;
      sequence_length = 1;
    } else {
      for (std::size_t j = 1; j < sequence_length; ++j) {
        const auto continuation = static_cast<unsigned char>(string[i + j]);
        if ((continuation >> 6) != 0x2) {
          code_point = 0xfffd;
          sequence_length = 1;
          break;
        }
        code_point = (code_point << 6) | (continuation & 0x3f);
      }
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
  return env->NewString(units.data(), static_cast<jsize>(units.size()));
}

jbyteArray to_jbytes(JNIEnv *env, const std::string_view bytes) {
  const auto length = static_cast<jsize>(bytes.size());
  jbyteArray result = env->NewByteArray(length);
  if (result == nullptr) {
    return nullptr;
  }
  env->SetByteArrayRegion(result, 0, length,
                          reinterpret_cast<const jbyte *>(bytes.data()));
  return result;
}

/// Throws @p class_name, constructed from `(String, int)`. False where the
/// class is absent.
bool throw_coded(JNIEnv *env, const char *class_name, const char *message,
                 const odr::ErrorCode code) {
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    env->ExceptionClear();
    return false;
  }
  const jmethodID constructor =
      env->GetMethodID(cls, "<init>", "(Ljava/lang/String;I)V");
  if (constructor == nullptr) {
    env->ExceptionClear();
    env->DeleteLocalRef(cls);
    return false;
  }
  jstring text = env->NewStringUTF(message);
  auto throwable = static_cast<jthrowable>(
      env->NewObject(cls, constructor, text, static_cast<jint>(code)));
  env->DeleteLocalRef(text);
  env->DeleteLocalRef(cls);
  if (throwable == nullptr) {
    return false; // an OutOfMemoryError is pending instead
  }
  env->Throw(throwable);
  env->DeleteLocalRef(throwable);
  return true;
}

void throw_java(JNIEnv *env) {
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
}

} // namespace odr_jni
