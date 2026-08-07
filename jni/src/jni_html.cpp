#include "jni_convert.hpp"
#include "odr_jni.hpp"

#include <odr/document.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/html.hpp>

#include <sstream>
#include <utility>
#include <vector>

namespace {

using odr_jni::destroy_handle;
using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::HandleGuard;
using odr_jni::make_handle;
using odr_jni::to_jbytes;
using odr_jni::to_jstring;
using odr_jni::to_string;

jlongArray to_jlong_array(JNIEnv *env, const std::vector<jlong> &values) {
  jlongArray result = env->NewLongArray(static_cast<jsize>(values.size()));
  if (result == nullptr) {
    return nullptr;
  }
  env->SetLongArrayRegion(result, 0, static_cast<jsize>(values.size()),
                          values.data());
  return result;
}

jlongArray wrap_views(JNIEnv *env, const odr::HtmlViews &views) {
  HandleGuard<odr::HtmlView> guard(views.size());
  for (const odr::HtmlView &view : views) {
    guard.add(view);
  }
  jlongArray result = to_jlong_array(env, guard.handles());
  if (result == nullptr) {
    return nullptr;
  }
  guard.release();
  return result;
}

/// Builds an `app.opendocument.core.Html` from an offline result.
jobject make_html(JNIEnv *env, odr::Html html) {
  jobject config = odr_jni::html_config_to_java(env, html.config());

  jclass page_cls = env->FindClass("app/opendocument/core/HtmlPage");
  if (page_cls == nullptr) {
    return nullptr;
  }
  jmethodID page_ctor = env->GetMethodID(
      page_cls, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
  if (page_ctor == nullptr) {
    return nullptr;
  }
  const std::vector<odr::HtmlPage> &pages = html.pages();
  jobjectArray page_array =
      env->NewObjectArray(static_cast<jsize>(pages.size()), page_cls, nullptr);
  for (jsize i = 0; i < static_cast<jsize>(pages.size()); ++i) {
    jstring name = to_jstring(env, pages[i].name);
    jstring path = to_jstring(env, pages[i].path);
    jobject page = env->NewObject(page_cls, page_ctor, name, path);
    env->SetObjectArrayElement(page_array, i, page);
    env->DeleteLocalRef(page);
    env->DeleteLocalRef(path);
    env->DeleteLocalRef(name);
  }
  env->DeleteLocalRef(page_cls);

  jclass html_cls = env->FindClass("app/opendocument/core/Html");
  if (html_cls == nullptr) {
    return nullptr;
  }
  jmethodID html_ctor = env->GetMethodID(
      html_cls, "<init>",
      "(Lapp/opendocument/core/HtmlConfig;[Lapp/opendocument/core/HtmlPage;)V");
  if (html_ctor == nullptr) {
    return nullptr;
  }
  jobject result = env->NewObject(html_cls, html_ctor, config, page_array);
  env->DeleteLocalRef(html_cls);
  return result;
}

/// Builds an `app.opendocument.core.Html.Content` from rendered HTML +
/// resources.
jobject make_content(JNIEnv *env, const std::string &html,
                     const odr::HtmlResources &resources) {
  jclass located_cls =
      env->FindClass("app/opendocument/core/Html$LocatedResource");
  if (located_cls == nullptr) {
    return nullptr;
  }
  jmethodID located_ctor = env->GetMethodID(
      located_cls, "<init>",
      "(Lapp/opendocument/core/HtmlResource;Ljava/lang/String;)V");
  if (located_ctor == nullptr) {
    return nullptr;
  }
  jclass resource_cls = env->FindClass("app/opendocument/core/HtmlResource");
  if (resource_cls == nullptr) {
    return nullptr;
  }
  jmethodID resource_ctor = env->GetMethodID(resource_cls, "<init>", "(J)V");
  if (resource_ctor == nullptr) {
    return nullptr;
  }

  jobjectArray located_array = env->NewObjectArray(
      static_cast<jsize>(resources.size()), located_cls, nullptr);
  for (jsize i = 0; i < static_cast<jsize>(resources.size()); ++i) {
    const auto &[resource, location] = resources[i];
    HandleGuard<odr::HtmlResource> guard(1);
    jobject resource_obj =
        env->NewObject(resource_cls, resource_ctor, guard.add(resource));
    if (resource_obj == nullptr) {
      return nullptr;
    }
    guard.release();
    jstring location_str =
        location.has_value() ? to_jstring(env, *location) : nullptr;
    jobject located =
        env->NewObject(located_cls, located_ctor, resource_obj, location_str);
    env->SetObjectArrayElement(located_array, i, located);
    env->DeleteLocalRef(located);
    if (location_str != nullptr) {
      env->DeleteLocalRef(location_str);
    }
    env->DeleteLocalRef(resource_obj);
  }
  env->DeleteLocalRef(resource_cls);
  env->DeleteLocalRef(located_cls);

  jclass content_cls = env->FindClass("app/opendocument/core/Html$Content");
  if (content_cls == nullptr) {
    return nullptr;
  }
  jmethodID content_ctor = env->GetMethodID(
      content_cls, "<init>",
      "(Ljava/lang/String;[Lapp/opendocument/core/Html$LocatedResource;)V");
  if (content_ctor == nullptr) {
    return nullptr;
  }
  jobject result = env->NewObject(content_cls, content_ctor,
                                  to_jstring(env, html), located_array);
  env->DeleteLocalRef(content_cls);
  return result;
}

odr::HtmlService &service(jlong handle) {
  return *from_handle<odr::HtmlService>(handle);
}

odr::HtmlView &view(jlong handle) {
  return *from_handle<odr::HtmlView>(handle);
}

odr::HtmlResource &resource(jlong handle) {
  return *from_handle<odr::HtmlResource>(handle);
}

} // namespace

// app.opendocument.core.Html (static entry points)

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Html_translateFile(JNIEnv *env, jclass,
                                              jlong file_handle,
                                              jstring cache_path,
                                              jobject config) {
  return guarded(env, [&] {
    return make_handle(odr::html::translate(
        *from_handle<odr::DecodedFile>(file_handle), to_string(env, cache_path),
        odr_jni::html_config_from_java(env, config)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Html_translateDocument(JNIEnv *env, jclass,
                                                  jlong document_handle,
                                                  jstring cache_path,
                                                  jobject config) {
  return guarded(env, [&] {
    return make_handle(
        odr::html::translate(*from_handle<odr::Document>(document_handle),
                             to_string(env, cache_path),
                             odr_jni::html_config_from_java(env, config)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Html_translateFilesystem(JNIEnv *env, jclass,
                                                    jlong filesystem_handle,
                                                    jstring cache_path,
                                                    jobject config) {
  return guarded(env, [&] {
    return make_handle(
        odr::html::translate(*from_handle<odr::Filesystem>(filesystem_handle),
                             to_string(env, cache_path),
                             odr_jni::html_config_from_java(env, config)));
  });
}

// app.opendocument.core.HtmlService

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HtmlService_destroy(JNIEnv *env, jclass,
                                               jlong handle) {
  destroy_handle<odr::HtmlService>(env, handle);
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlService_configNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    return odr_jni::html_config_to_java(env, service(handle).config());
  });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_HtmlService_listViewsNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env,
                 [&] { return wrap_views(env, service(handle).list_views()); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HtmlService_warmupNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  guarded(env, [&] { service(handle).warmup(); });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_HtmlService_existsNative(JNIEnv *env, jobject,
                                                    jlong handle,
                                                    jstring path) {
  return guarded(env, [&] {
    return static_cast<jboolean>(service(handle).exists(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlService_mimetypeNative(JNIEnv *env, jobject,
                                                      jlong handle,
                                                      jstring path) {
  return guarded(env, [&] {
    return to_jstring(env, service(handle).mimetype(to_string(env, path)));
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_HtmlService_writeNative(JNIEnv *env, jobject,
                                                   jlong handle, jstring path) {
  return guarded(env, [&] {
    std::ostringstream out;
    service(handle).write(to_string(env, path), out);
    return to_jbytes(env, out.str());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlService_writeHtmlNative(JNIEnv *env, jobject,
                                                       jlong handle,
                                                       jstring path) {
  return guarded(env, [&] {
    std::ostringstream out;
    odr::HtmlResources resources =
        service(handle).write_html(to_string(env, path), out);
    return make_content(env, out.str(), resources);
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlService_bringOfflineNative(JNIEnv *env, jobject,
                                                          jlong handle,
                                                          jstring output_path) {
  return guarded(env, [&] {
    return make_html(
        env, service(handle).bring_offline(to_string(env, output_path)));
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlService_bringOfflineViewsNative(
    JNIEnv *env, jobject, jlong handle, jstring output_path,
    jlongArray view_handles) {
  return guarded(env, [&] {
    std::vector<odr::HtmlView> views;
    const jsize length = env->GetArrayLength(view_handles);
    jlong *handles = env->GetLongArrayElements(view_handles, nullptr);
    for (jsize i = 0; i < length; ++i) {
      views.push_back(*from_handle<odr::HtmlView>(handles[i]));
    }
    env->ReleaseLongArrayElements(view_handles, handles, JNI_ABORT);
    return make_html(
        env, service(handle).bring_offline(to_string(env, output_path), views));
  });
}

// app.opendocument.core.HtmlView

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HtmlView_destroy(JNIEnv *env, jclass, jlong handle) {
  destroy_handle<odr::HtmlView>(env, handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlView_nameNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] { return to_jstring(env, view(handle).name()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_HtmlView_indexNative(JNIEnv *env, jobject,
                                                jlong handle) {
  return guarded(env, [&] { return static_cast<jlong>(view(handle).index()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlView_pathNative(JNIEnv *env, jobject,
                                               jlong handle) {
  return guarded(env, [&] { return to_jstring(env, view(handle).path()); });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlView_configNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  return guarded(env, [&] {
    return odr_jni::html_config_to_java(env, view(handle).config());
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlView_writeHtmlNative(JNIEnv *env, jobject,
                                                    jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    odr::HtmlResources resources = view(handle).write_html(out);
    return make_content(env, out.str(), resources);
  });
}

extern "C" JNIEXPORT jobject JNICALL
Java_app_opendocument_core_HtmlView_bringOfflineNative(JNIEnv *env, jobject,
                                                       jlong handle,
                                                       jstring output_path) {
  return guarded(env, [&] {
    return make_html(env,
                     view(handle).bring_offline(to_string(env, output_path)));
  });
}

// app.opendocument.core.HtmlResource

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HtmlResource_destroy(JNIEnv *env, jclass,
                                                jlong handle) {
  destroy_handle<odr::HtmlResource>(env, handle);
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_HtmlResource_typeNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env,
                 [&] { return static_cast<jint>(resource(handle).type()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlResource_mimeTypeNative(JNIEnv *env, jobject,
                                                       jlong handle) {
  return guarded(env,
                 [&] { return to_jstring(env, resource(handle).mime_type()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlResource_nameNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] { return to_jstring(env, resource(handle).name()); });
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HtmlResource_pathNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] { return to_jstring(env, resource(handle).path()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_HtmlResource_fileNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&]() -> jlong {
    const std::optional<odr::File> &file = resource(handle).file();
    if (!file.has_value()) {
      return 0;
    }
    return make_handle(odr::File(*file));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_HtmlResource_isShippedNative(JNIEnv *env, jobject,
                                                        jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(resource(handle).is_shipped());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_HtmlResource_isExternalNative(JNIEnv *env, jobject,
                                                         jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(resource(handle).is_external());
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_HtmlResource_isAccessibleNative(JNIEnv *env, jobject,
                                                           jlong handle) {
  return guarded(env, [&] {
    return static_cast<jboolean>(resource(handle).is_accessible());
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_app_opendocument_core_HtmlResource_readNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  return guarded(env, [&] {
    std::ostringstream out;
    resource(handle).write_resource(out);
    return to_jbytes(env, out.str());
  });
}
