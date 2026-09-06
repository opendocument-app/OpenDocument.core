#include "jni_convert.hpp"
#include "odr_jni.hpp"

#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_position.hpp>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::make_file_type_capabilities;
using odr_jni::make_handle;
using odr_jni::to_jstring;
using odr_jni::to_string;

jintArray to_jint_array(JNIEnv *env, const std::vector<jint> &values) {
  jintArray result = env->NewIntArray(static_cast<jsize>(values.size()));
  if (result == nullptr) {
    return nullptr;
  }
  env->SetIntArrayRegion(result, 0, static_cast<jsize>(values.size()),
                         values.data());
  return result;
}

jobjectArray to_jstring_array(JNIEnv *env,
                              const std::span<const std::string_view> values) {
  jclass cls = env->FindClass("java/lang/String");
  if (cls == nullptr) {
    return nullptr;
  }
  jobjectArray result =
      env->NewObjectArray(static_cast<jsize>(values.size()), cls, nullptr);
  env->DeleteLocalRef(cls);
  if (result == nullptr) {
    return nullptr;
  }
  for (std::size_t i = 0; i < values.size(); ++i) {
    jstring value = to_jstring(env, values[i]);
    env->SetObjectArrayElement(result, static_cast<jsize>(i), value);
    env->DeleteLocalRef(value);
  }
  return result;
}

} // namespace

// app.opendocument.core.Odr

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_version(JNIEnv *env, jclass) {
  return guarded(env, [&] { return to_jstring(env, odr::version()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_commitHash(JNIEnv *env, jclass) {
  return guarded(env, [&] { return to_jstring(env, odr::commit_hash()); });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Odr_isDirty(JNIEnv *env, jclass) {
  return guarded(env, [&] { return static_cast<jboolean>(odr::is_dirty()); });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Odr_isDebug(JNIEnv *env, jclass) {
  return guarded(env, [&] { return static_cast<jboolean>(odr::is_debug()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_identify(JNIEnv *env, jclass) {
  return guarded(env, [&] { return to_jstring(env, odr::identify()); });
}

extern "C" JNIEXPORT jintArray JNICALL
Java_app_opendocument_core_Odr_allFileTypesNative(JNIEnv *env, jclass) {
  return guarded(env, [&] {
    std::vector<jint> codes;
    for (const odr::FileType type : odr::all_file_types()) {
      codes.push_back(static_cast<jint>(type));
    }
    return to_jint_array(env, codes);
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Odr_fileTypeByFileExtensionNative(
    JNIEnv *env, jclass, jstring extension) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::file_type_by_file_extension(to_string(env, extension)));
  });
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_app_opendocument_core_Odr_fileExtensionsByFileTypeNative(JNIEnv *env,
                                                              jclass,
                                                              jint type) {
  return guarded(env, [&] {
    return to_jstring_array(env, odr::file_extensions_by_file_type(
                                     static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_fileExtensionByFileTypeNative(JNIEnv *env,
                                                             jclass,
                                                             jint type) {
  return guarded(env, [&] {
    return to_jstring(env, odr::file_extension_by_file_type(
                               static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Odr_fileCategoryByFileTypeNative(JNIEnv *env, jclass,
                                                            jint type) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::file_category_by_file_type(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Odr_documentTypeByFileTypeNative(JNIEnv *env, jclass,
                                                            jint type) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::document_type_by_file_type(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_fileTypeToStringNative(JNIEnv *env, jclass,
                                                      jint type) {
  return guarded(env, [&] {
    return to_jstring(
        env, odr::file_type_to_string(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_fileCategoryToStringNative(JNIEnv *env, jclass,
                                                          jint category) {
  return guarded(env, [&] {
    return to_jstring(env, odr::file_category_to_string(
                               static_cast<odr::FileCategory>(category)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_documentTypeToStringNative(JNIEnv *env, jclass,
                                                          jint type) {
  return guarded(env, [&] {
    return to_jstring(env, odr::document_type_to_string(
                               static_cast<odr::DocumentType>(type)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Odr_fileTypeByMimetypeNative(JNIEnv *env, jclass,
                                                        jstring mimetype) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::file_type_by_mimetype(to_string(env, mimetype)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_mimetypeByFileTypeNative(JNIEnv *env, jclass,
                                                        jint type) {
  return guarded(env, [&] {
    return to_jstring(
        env, odr::mimetype_by_file_type(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_app_opendocument_core_Odr_mimetypesByFileTypeNative(JNIEnv *env, jclass,
                                                         jint type) {
  return guarded(env, [&] {
    return to_jstring_array(
        env, odr::mimetypes_by_file_type(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_Odr_capabilitiesByFileTypeNative(JNIEnv *env, jclass,
                                                            jint type) {
  return guarded(env, [&] {
    return make_file_type_capabilities(
        env, odr::capabilities_by_file_type(static_cast<odr::FileType>(type)));
  });
}

extern "C" JNIEXPORT jintArray JNICALL
Java_app_opendocument_core_Odr_listFileTypesNative(JNIEnv *env, jclass,
                                                   jstring path) {
  return guarded(env, [&] {
    std::vector<jint> codes;
    for (const odr::FileType type :
         odr::list_file_types(to_string(env, path))) {
      codes.push_back(static_cast<jint>(type));
    }
    return to_jint_array(env, codes);
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_Odr_mimetype(JNIEnv *env, jclass, jstring path) {
  return guarded(env, [&] {
    return to_jstring(env, odr::mimetype(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Odr_openNative(JNIEnv *env, jclass, jstring path) {
  return guarded(env,
                 [&] { return make_handle(odr::open(to_string(env, path))); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Odr_openWithLoggerNative(JNIEnv *env, jclass,
                                                    jstring path,
                                                    jlong logger) {
  return guarded(env, [&] {
    return make_handle(
        odr::open(to_string(env, path), {}, *from_handle<odr::Logger>(logger)));
  });
}

extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_Odr_openAsNative(
    JNIEnv *env, jclass, jstring path, jint as) {
  return guarded(env, [&] {
    return make_handle(
        odr::open(to_string(env, path),
                  odr::DecodeOptions::as(static_cast<odr::FileType>(as))));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Odr_openWithOptionsNative(
    JNIEnv *env, jclass, jstring path, jint as_file_type,
    jintArray file_type_priority, jint csv_encoding, jint csv_separator,
    jint csv_quote) {
  return guarded(env, [&] {
    odr::DecodeOptions options;
    if (as_file_type >= 0) {
      options.as_file_type = static_cast<odr::FileType>(as_file_type);
    }
    if (jint *codes = env->GetIntArrayElements(file_type_priority, nullptr);
        codes != nullptr) {
      const jsize length = env->GetArrayLength(file_type_priority);
      for (jsize i = 0; i < length; ++i) {
        options.file_type_priority.push_back(
            static_cast<odr::FileType>(codes[i]));
      }
      env->ReleaseIntArrayElements(file_type_priority, codes, JNI_ABORT);
    }
    // -1 is how java spells an unset field across the boundary
    if (csv_encoding >= 0) {
      options.csv.encoding = static_cast<odr::TextEncoding>(csv_encoding);
    }
    if (csv_separator >= 0) {
      options.csv.separator = static_cast<char>(csv_separator);
    }
    if (csv_quote >= 0) {
      options.csv.quote = static_cast<char>(csv_quote);
    }
    return make_handle(odr::open(to_string(env, path), options));
  });
}

// app.opendocument.core.TextEncoding

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_TextEncoding_toStringNative(JNIEnv *env, jclass,
                                                       jint encoding) {
  return guarded(env, [&] {
    return to_jstring(env, odr::text_encoding_to_string(
                               static_cast<odr::TextEncoding>(encoding)));
  });
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_app_opendocument_core_TextEncoding_namesNative(JNIEnv *env, jclass,
                                                    jint encoding) {
  return guarded(env, [&] {
    return to_jstring_array(env, odr::text_encoding_names(
                                     static_cast<odr::TextEncoding>(encoding)));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_TextEncoding_isDecodableNative(JNIEnv *env, jclass,
                                                          jint encoding) {
  return guarded(env, [&] {
    return static_cast<jboolean>(odr::text_encoding_is_decodable(
        static_cast<odr::TextEncoding>(encoding)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_TextEncoding_byNameNative(JNIEnv *env, jclass,
                                                     jstring name) {
  return guarded(env, [&] {
    return static_cast<jint>(odr::text_encoding_by_name(to_string(env, name)));
  });
}

extern "C" JNIEXPORT jintArray JNICALL
Java_app_opendocument_core_TextEncoding_allNative(JNIEnv *env, jclass) {
  return guarded(env, [&] {
    std::vector<jint> codes;
    for (const odr::TextEncoding encoding : odr::all_text_encodings()) {
      codes.push_back(static_cast<jint>(encoding));
    }
    return to_jint_array(env, codes);
  });
}

// app.opendocument.core.TablePosition

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_TablePosition_toColumnNum(JNIEnv *env, jclass,
                                                     jstring string) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::TablePosition::to_column_num(to_string(env, string)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_TablePosition_toRowNum(JNIEnv *env, jclass,
                                                  jstring string) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::TablePosition::to_row_num(to_string(env, string)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_TablePosition_toColumnString(JNIEnv *env, jclass,
                                                        jint column) {
  return guarded(env, [&] {
    return to_jstring(env, odr::TablePosition::to_column_string(
                               static_cast<std::uint32_t>(column)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_TablePosition_toRowString(JNIEnv *env, jclass,
                                                     jint row) {
  return guarded(env, [&] {
    return to_jstring(env, odr::TablePosition::to_row_string(
                               static_cast<std::uint32_t>(row)));
  });
}
