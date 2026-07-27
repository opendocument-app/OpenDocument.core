#include "odr_jni.hpp"

#ifdef ODR_WITH_HTTP_SERVER

#include "jni_convert.hpp"

#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/http_server.hpp>

#include <vector>

namespace {

using odr_jni::destroy_handle;
using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::make_handle;
using odr_jni::to_jstring;
using odr_jni::to_string;

odr::HttpServer &server(jlong handle) {
  return *from_handle<odr::HttpServer>(handle);
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Odr_hasHttpServer(JNIEnv *, jclass) {
  return JNI_TRUE;
}

extern "C" JNIEXPORT jlong JNICALL Java_app_opendocument_core_HttpServer_create(
    JNIEnv *env, jclass, jstring cache_path) {
  return guarded(env, [&] {
    odr::HttpServer::Config config;
    config.cache_path = to_string(env, cache_path);
    return make_handle(odr::HttpServer(config));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_destroy(JNIEnv *, jclass, jlong handle) {
  destroy_handle<odr::HttpServer>(handle);
}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HttpServer_cachePathNative(JNIEnv *env, jobject,
                                                      jlong handle) {
  return guarded(
      env, [&] { return to_jstring(env, server(handle).config().cache_path); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_connectServiceNative(JNIEnv *env, jobject,
                                                           jlong handle,
                                                           jlong service_handle,
                                                           jstring prefix) {
  guarded(env, [&] {
    server(handle).connect_service(
        *from_handle<odr::HtmlService>(service_handle), to_string(env, prefix));
  });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_HttpServer_serveFileNative(JNIEnv *env, jobject,
                                                      jlong handle,
                                                      jlong file_handle,
                                                      jstring prefix,
                                                      jobject config) {
  return guarded(env, [&] {
    const odr::HtmlViews views = server(handle).serve_file(
        *from_handle<odr::DecodedFile>(file_handle), to_string(env, prefix),
        odr_jni::html_config_from_java(env, config));
    std::vector<jlong> handles;
    handles.reserve(views.size());
    for (const odr::HtmlView &view : views) {
      handles.push_back(make_handle(odr::HtmlView(view)));
    }
    jlongArray result = env->NewLongArray(static_cast<jsize>(handles.size()));
    if (result == nullptr) {
      return jlongArray{};
    }
    env->SetLongArrayRegion(result, 0, static_cast<jsize>(handles.size()),
                            handles.data());
    return result;
  });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_HttpServer_bindNative(JNIEnv *env, jobject,
                                                 jlong handle, jstring host,
                                                 jint port,
                                                 jboolean reuse_address,
                                                 jboolean reuse_port) {
  return guarded(env, [&] {
    odr::HttpServer::Options options;
    options.reuse_address = reuse_address == JNI_TRUE;
    options.reuse_port = reuse_port == JNI_TRUE;
    return static_cast<jint>(server(handle).bind(
        to_string(env, host), static_cast<std::uint32_t>(port), options));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_listenNative(JNIEnv *env, jobject,
                                                   jlong handle) {
  guarded(env, [&] { server(handle).listen(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_clearNative(JNIEnv *env, jobject,
                                                  jlong handle) {
  guarded(env, [&] { server(handle).clear(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_stopNative(JNIEnv *env, jobject,
                                                 jlong handle) {
  guarded(env, [&] { server(handle).stop(); });
}

#else

#include <odr/exceptions.hpp>

namespace {

[[noreturn]] void unsupported() {
  throw odr::UnsupportedOperation("built without HTTP server");
}

} // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Odr_hasHttpServer(JNIEnv *, jclass) {
  return JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_HttpServer_create(JNIEnv *env, jclass, jstring) {
  return odr_jni::guarded(env, [&]() -> jlong { unsupported(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_destroy(JNIEnv *, jclass, jlong) {}

extern "C" JNIEXPORT jstring JNICALL
Java_app_opendocument_core_HttpServer_cachePathNative(JNIEnv *env, jobject,
                                                      jlong) {
  return odr_jni::guarded(env, [&]() -> jstring { unsupported(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_connectServiceNative(JNIEnv *env, jobject,
                                                           jlong, jlong,
                                                           jstring) {
  odr_jni::guarded(env, [&] { unsupported(); });
}

extern "C" JNIEXPORT jlongArray JNICALL
Java_app_opendocument_core_HttpServer_serveFileNative(JNIEnv *env, jobject,
                                                      jlong, jlong, jstring,
                                                      jobject) {
  return odr_jni::guarded(env, [&]() -> jlongArray { unsupported(); });
}

extern "C" JNIEXPORT jint JNICALL
Java_app_opendocument_core_HttpServer_bindNative(JNIEnv *env, jobject, jlong,
                                                 jstring, jint, jboolean,
                                                 jboolean) {
  return odr_jni::guarded(env, [&]() -> jint { unsupported(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_listenNative(JNIEnv *env, jobject,
                                                   jlong) {
  odr_jni::guarded(env, [&] { unsupported(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_clearNative(JNIEnv *env, jobject, jlong) {
  odr_jni::guarded(env, [&] { unsupported(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_HttpServer_stopNative(JNIEnv *env, jobject, jlong) {
  odr_jni::guarded(env, [&] { unsupported(); });
}

#endif
