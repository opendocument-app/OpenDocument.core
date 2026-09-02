#include "jni_convert.hpp"
#include "odr_jni.hpp"

#include <odr/archive.hpp>
#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <optional>
#include <sstream>

namespace {

using odr_jni::destroy_handle;
using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::make_handle;
using odr_jni::to_jbytes;
using odr_jni::to_jstring;
using odr_jni::to_string;

odr::DecodedFile &decoded(jlong handle) {
  return *from_handle<odr::DecodedFile>(handle);
}

} // namespace

// app.opendocument.core.File

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_File_create(JNIEnv *env, jclass, jstring path) {
  return guarded(env,
                 [&] { return make_handle(odr::File(to_string(env, path))); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_File_destroy(JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::File>(env, handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_File_locationNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(from_handle<odr::File>(handle)->location());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_File_sizeNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return static_cast<jlong>(from_handle<odr::File>(handle)->size());
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_File_diskPathNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_string_opt(
        env, from_handle<odr::File>(handle)->disk_path());
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_File_readNative(JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    from_handle<odr::File>(handle)->pipe(out);
    return to_jbytes(env, out.str());
  });
}

extern "C" JNIEXPORT void JNICALL Java_app_opendocument_core_File_copyNative(
    JNIEnv *env, jobject, jlong handle, jstring path) {
  guarded(env,
          [&] { from_handle<odr::File>(handle)->copy(to_string(env, path)); });
}

// Yields a DecodedFile but belongs to File: a native taking a handle must be an
// instance method of its owner, or the wrapper can be collected mid-call.
extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_File_decodeNative(
    JNIEnv *env, jobject, jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(*from_handle<odr::File>(handle)));
  });
}

// app.opendocument.core.DecodedFile
//
// Handles hold a heap `odr::DecodedFile` (the typed C++ subclasses are sliced
// away); typed accessors re-derive the typed view per call via `as_*`.

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_create(JNIEnv *env, jclass,
                                              jstring path) {
  return guarded(
      env, [&] { return make_handle(odr::DecodedFile(to_string(env, path))); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_createAs(JNIEnv *env, jclass,
                                                jstring path, jint as) {
  return guarded(env, [&] {
    return make_handle(
        odr::DecodedFile(to_string(env, path), static_cast<odr::FileType>(as)));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_DecodedFile_destroy(JNIEnv *env, jclass,
                                               jlong handle) {
  destroy_handle<odr::DecodedFile>(env, handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_fileNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] { return make_handle(decoded(handle).file()); });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_DecodedFile_fileTypeNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(
      env, [&] { return static_cast<jint>(decoded(handle).file_type()); });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_DecodedFile_fileCategoryNative(JNIEnv *env, jobject,
                                                          jlong handle) {
  return guarded(
      env, [&] { return static_cast<jint>(decoded(handle).file_category()); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_DecodedFile_fileMetaNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_file_meta(env, decoded(handle).file_meta());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_passwordEncryptedNative(JNIEnv *env,
                                                               jobject,
                                                               jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).password_encrypted());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_DecodedFile_encryptionStateNative(JNIEnv *env,
                                                             jobject,
                                                             jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(decoded(handle).encryption_state());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_decryptNative(JNIEnv *env, jobject,
                                                     jlong handle,
                                                     jstring password) {
  return guarded(env, [&] {
    return make_handle(decoded(handle).decrypt(to_string(env, password)));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isDecodableNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_decodable());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_DecodedFile_capabilitiesNative(JNIEnv *env, jobject,
                                                          jlong handle) {
  return guarded(env, [&] {
    return odr_jni::make_file_type_capabilities(env,
                                                decoded(handle).capabilities());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isTextFileNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_text_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isImageFileNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_image_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isArchiveFileNative(JNIEnv *env, jobject,
                                                           jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_archive_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isDocumentFileNative(JNIEnv *env,
                                                            jobject,
                                                            jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_document_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isPdfFileNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_pdf_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_DecodedFile_isFontFileNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(decoded(handle).is_font_file());
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asTextFileNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_text_file()));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asImageFileNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_image_file()));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asArchiveFileNative(JNIEnv *env, jobject,
                                                           jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_archive_file()));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asDocumentFileNative(JNIEnv *env,
                                                            jobject,
                                                            jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_document_file()));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asPdfFileNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_pdf_file()));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DecodedFile_asFontFileNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(decoded(handle).as_font_file()));
  });
}

// app.opendocument.core.TextFile

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_TextFile_charsetNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    const odr::TextEncoding encoding =
        decoded(handle).as_text_file().encoding();
    return odr_jni::make_string_opt(
        env, encoding == odr::TextEncoding::unknown
                 ? std::optional<std::string>{}
                 : std::string(odr::text_encoding_to_string(encoding)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_TextFile_textNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, decoded(handle).as_text_file().text());
  });
}

// app.opendocument.core.ImageFile

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_ImageFile_readNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    out << decoded(handle).as_image_file().stream()->rdbuf();
    return to_jbytes(env, out.str());
  });
}

// app.opendocument.core.ArchiveFile

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_ArchiveFile_archiveNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  return guarded(env, [&] {
    return make_handle(decoded(handle).as_archive_file().archive());
  });
}

// app.opendocument.core.DocumentFile

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentFile_create(JNIEnv *env, jclass,
                                               jstring path) {
  return guarded(env, [&] {
    return make_handle(
        odr::DecodedFile(odr::DocumentFile(to_string(env, path))));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_DocumentFile_typeByPathNative(JNIEnv *env, jclass,
                                                         jstring path) {
  return guarded(env, [&] {
    return static_cast<jint>(odr::DocumentFile::type(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_DocumentFile_metaByPathNative(JNIEnv *env, jclass,
                                                         jstring path) {
  return guarded(env, [&] {
    return odr_jni::make_file_meta(
        env, odr::DocumentFile::meta(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_DocumentFile_documentTypeNative(JNIEnv *env, jobject,
                                                           jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(
        decoded(handle).as_document_file().document_type());
  });
}

/// 0 where there is no thumbnail; `DocumentFile.thumbnail` maps that to null.
extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentFile_thumbnailNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&]() -> jlong {
    const std::optional<odr::File> thumbnail =
        decoded(handle).as_document_file().thumbnail();
    return thumbnail.has_value() ? make_handle(*thumbnail) : 0;
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentFile_decryptDocumentFileNative(
    JNIEnv *env, jobject, jlong handle, jstring password) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(
        decoded(handle).as_document_file().decrypt(to_string(env, password))));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_DocumentFile_documentNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env, [&] {
    return make_handle(decoded(handle).as_document_file().document());
  });
}

// app.opendocument.core.PdfFile

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_PdfFile_decryptPdfFileNative(JNIEnv *env, jobject,
                                                        jlong handle,
                                                        jstring password) {
  return guarded(env, [&] {
    return make_handle(odr::DecodedFile(
        decoded(handle).as_pdf_file().decrypt(to_string(env, password))));
  });
}

// app.opendocument.core.FontFile

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_FontFile_readNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    out << decoded(handle).as_font_file().stream()->rdbuf();
    return to_jbytes(env, out.str());
  });
}

// app.opendocument.core.FileWalker

extern "C" JNIEXPORT void JNICALL Java_app_opendocument_core_FileWalker_destroy(
    JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::FileWalker>(env, handle);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_FileWalker_endNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(from_handle<odr::FileWalker>(handle)->end());
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_FileWalker_depthNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  return guarded(env, [&] {
    return static_cast<jint>(from_handle<odr::FileWalker>(handle)->depth());
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_FileWalker_pathNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return to_jstring(env, from_handle<odr::FileWalker>(handle)->path());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_FileWalker_isFileNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::FileWalker>(handle)->is_file());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_FileWalker_isDirectoryNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::FileWalker>(handle)->is_directory());
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_FileWalker_popNative(JNIEnv *env, jobject,
                                                jlong handle) {
  guarded(env, [&] { from_handle<odr::FileWalker>(handle)->pop(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_FileWalker_nextNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  guarded(env, [&] { from_handle<odr::FileWalker>(handle)->next(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_FileWalker_flatNextNative(JNIEnv *env, jobject,
                                                     jlong handle) {
  guarded(env, [&] { from_handle<odr::FileWalker>(handle)->flat_next(); });
}

// app.opendocument.core.Filesystem

extern "C" JNIEXPORT void JNICALL Java_app_opendocument_core_Filesystem_destroy(
    JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::Filesystem>(env, handle);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Filesystem_existsNative(JNIEnv *env, jobject,
                                                   jlong handle, jstring path) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::Filesystem>(handle)->exists(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Filesystem_isFileNative(JNIEnv *env, jobject,
                                                   jlong handle, jstring path) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::Filesystem>(handle)->is_file(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Filesystem_isDirectoryNative(JNIEnv *env, jobject,
                                                        jlong handle,
                                                        jstring path) {
  return guarded(env, [&] {
    return static_cast<jboolean>(
        from_handle<odr::Filesystem>(handle)->is_directory(
            to_string(env, path)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Filesystem_fileWalkerNative(JNIEnv *env, jobject,
                                                       jlong handle,
                                                       jstring path) {
  return guarded(env, [&] {
    return make_handle(from_handle<odr::Filesystem>(handle)->file_walker(
        to_string(env, path)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Filesystem_openNative(JNIEnv *env, jobject,
                                                 jlong handle, jstring path) {
  return guarded(env, [&] {
    return make_handle(
        from_handle<odr::Filesystem>(handle)->open(to_string(env, path)));
  });
}

// app.opendocument.core.Archive

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Archive_destroy(JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::Archive>(env, handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Archive_asFilesystemNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(env, [&] {
    return make_handle(from_handle<odr::Archive>(handle)->as_filesystem());
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_Archive_saveNative(JNIEnv *env, jobject,
                                              jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    from_handle<odr::Archive>(handle)->save(out);
    return to_jbytes(env, out.str());
  });
}
