#include "odr_jni.hpp"

#include <odr/exceptions.hpp>

#include <cstdint>
#include <vector>

namespace odr_jni {

namespace {

void throw_new(JNIEnv *env, const char *class_name, const char *message) {
  jclass cls = env->FindClass(class_name);
  if (cls == nullptr) {
    return; // a NoClassDefFoundError is pending instead
  }
  env->ThrowNew(cls, message);
  env->DeleteLocalRef(cls);
}

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

void throw_java(JNIEnv *env) {
  constexpr auto base = "app/opendocument/core/OdrException";
  try {
    throw;
  } catch (const odr::UnsupportedOperation &e) {
    throw_new(env, "app/opendocument/core/OdrException$UnsupportedOperation",
              e.what());
  } catch (const odr::FileNotFound &e) {
    throw_new(env, "app/opendocument/core/OdrException$FileNotFound", e.what());
  } catch (const odr::UnknownFileType &e) {
    throw_new(env, "app/opendocument/core/OdrException$UnknownFileType",
              e.what());
  } catch (const odr::UnsupportedFileType &e) {
    throw_new(env, "app/opendocument/core/OdrException$UnsupportedFileType",
              e.what());
  } catch (const odr::FileReadError &e) {
    throw_new(env, "app/opendocument/core/OdrException$FileReadError",
              e.what());
  } catch (const odr::FileWriteError &e) {
    throw_new(env, "app/opendocument/core/OdrException$FileWriteError",
              e.what());
  } catch (const odr::NoDocumentFile &e) {
    throw_new(env, "app/opendocument/core/OdrException$NoDocumentFile",
              e.what());
  } catch (const odr::UnknownDocumentType &e) {
    throw_new(env, "app/opendocument/core/OdrException$UnknownDocumentType",
              e.what());
  } catch (const odr::UnsupportedCryptoAlgorithm &e) {
    throw_new(env,
              "app/opendocument/core/OdrException$UnsupportedCryptoAlgorithm",
              e.what());
  } catch (const odr::WrongPasswordError &e) {
    throw_new(env, "app/opendocument/core/OdrException$WrongPassword",
              e.what());
  } catch (const odr::DecryptionFailed &e) {
    throw_new(env, "app/opendocument/core/OdrException$DecryptionFailed",
              e.what());
  } catch (const odr::NotEncryptedError &e) {
    throw_new(env, "app/opendocument/core/OdrException$NotEncrypted", e.what());
  } catch (const odr::FileEncryptedError &e) {
    throw_new(env, "app/opendocument/core/OdrException$FileEncrypted",
              e.what());
  } catch (const odr::DocumentCopyProtectedException &e) {
    throw_new(env, "app/opendocument/core/OdrException$DocumentCopyProtected",
              e.what());
  } catch (const std::exception &e) {
    throw_new(env, base, e.what());
  } catch (...) {
    throw_new(env, base, "unknown native error");
  }
}

} // namespace odr_jni
