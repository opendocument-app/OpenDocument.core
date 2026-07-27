#include "odr_jni.hpp"

#include <odr/file.hpp>
#include <odr/global_params.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_position.hpp>

#include <string>
#include <vector>

namespace {

using odr_jni::from_handle;
using odr_jni::guarded;
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

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_Odr_fileTypeByFileExtensionNative(
    JNIEnv *env, jclass, jstring extension) {
  return guarded(env, [&] {
    return static_cast<jint>(
        odr::file_type_by_file_extension(to_string(env, extension)));
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
        odr::open(to_string(env, path), *from_handle<odr::Logger>(logger)));
  });
}

extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_Odr_openAsNative(
    JNIEnv *env, jclass, jstring path, jint as) {
  return guarded(env, [&] {
    return make_handle(
        odr::open(to_string(env, path), static_cast<odr::FileType>(as)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Odr_openWithPreferenceNative(
    JNIEnv *env, jclass, jstring path, jint as_file_type,
    jintArray file_type_priority) {
  return guarded(env, [&] {
    odr::DecodePreference preference;
    if (as_file_type >= 0) {
      preference.as_file_type = static_cast<odr::FileType>(as_file_type);
    }
    const auto append_codes = [&](jintArray array, auto &target,
                                  auto transform) {
      const jsize length = env->GetArrayLength(array);
      jint *codes = env->GetIntArrayElements(array, nullptr);
      for (jsize i = 0; i < length; ++i) {
        target.push_back(transform(codes[i]));
      }
      env->ReleaseIntArrayElements(array, codes, JNI_ABORT);
    };
    append_codes(file_type_priority, preference.file_type_priority,
                 [](jint code) { return static_cast<odr::FileType>(code); });
    return make_handle(odr::open(to_string(env, path), preference));
  });
}

// app.opendocument.core.GlobalParams

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_GlobalParams_odrCoreDataPath(JNIEnv *env, jclass) {
  return guarded(env, [&] {
    return to_jstring(env, odr::GlobalParams::odr_core_data_path());
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_GlobalParams_libmagicDatabasePath(JNIEnv *env,
                                                             jclass) {
  return guarded(env, [&] {
    return to_jstring(env, odr::GlobalParams::libmagic_database_path());
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_GlobalParams_setOdrCoreDataPath(JNIEnv *env, jclass,
                                                           jstring path) {
  guarded(env, [&] {
    odr::GlobalParams::set_odr_core_data_path(to_string(env, path));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_GlobalParams_setLibmagicDatabasePath(JNIEnv *env,
                                                                jclass,
                                                                jstring path) {
  guarded(env, [&] {
    odr::GlobalParams::set_libmagic_database_path(to_string(env, path));
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
