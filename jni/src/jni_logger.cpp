#include "odr_jni.hpp"

#include <odr/logger.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace {

using odr_jni::destroy_handle;
using odr_jni::from_handle;
using odr_jni::guarded;
using odr_jni::make_handle;
using odr_jni::to_jstring;
using odr_jni::to_string;

/// `JavaVM::AttachCurrentThread` takes a `JNIEnv **` on android and a `void **`
/// on the jdk, and neither pointer converts to the other, so the argument has
/// to be typed per platform. `GetEnv` is `void **` on both and needs none of
/// this.
jint attach_current_thread(JavaVM *const vm, JNIEnv **const env) {
#ifdef __ANDROID__
  return vm->AttachCurrentThread(env, nullptr);
#else
  return vm->AttachCurrentThread(reinterpret_cast<void **>(env), nullptr);
#endif
}

/// A `JNIEnv` for the calling thread, attaching it if the JVM does not know it
/// yet. Log calls arrive on whatever thread the library happens to be working
/// on, which is not necessarily one the JVM started.
class ScopedEnv {
public:
  explicit ScopedEnv(JavaVM *vm) : m_vm{vm} {
    if (m_vm == nullptr) {
      return;
    }
    const jint status =
        m_vm->GetEnv(reinterpret_cast<void **>(&m_env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
      if (attach_current_thread(m_vm, &m_env) == JNI_OK) {
        m_attached = true;
      } else {
        m_env = nullptr;
      }
    } else if (status != JNI_OK) {
      m_env = nullptr;
    }
  }

  ScopedEnv(const ScopedEnv &) = delete;
  ScopedEnv &operator=(const ScopedEnv &) = delete;

  ~ScopedEnv() {
    if (m_attached) {
      m_vm->DetachCurrentThread();
    }
  }

  [[nodiscard]] JNIEnv *get() const { return m_env; }

private:
  JavaVM *m_vm{nullptr};
  JNIEnv *m_env{nullptr};
  bool m_attached{false};
};

/// Forwards to an `app.opendocument.core.ILogger`. Calls route through
/// `LoggerBridge`, whose static helpers do the enum and `SourceLocation`
/// construction, so only three method handles need caching here.
class JavaLogger final : public odr::ILogger {
public:
  JavaLogger(JNIEnv *env, jobject sink) {
    env->GetJavaVM(&m_vm);
    m_sink = env->NewGlobalRef(sink);

    jclass bridge = env->FindClass("app/opendocument/core/LoggerBridge");
    if (bridge == nullptr) {
      return;
    }
    m_bridge = static_cast<jclass>(env->NewGlobalRef(bridge));
    m_will_log = env->GetStaticMethodID(m_bridge, "willLog",
                                        "(Lapp/opendocument/core/ILogger;I)Z");
    m_log = env->GetStaticMethodID(m_bridge, "log",
                                   "(Lapp/opendocument/core/ILogger;JILjava/"
                                   "lang/String;Ljava/lang/String;Ljava/lang/"
                                   "String;I)V");
    m_flush = env->GetStaticMethodID(m_bridge, "flush",
                                     "(Lapp/opendocument/core/ILogger;)V");
  }

  JavaLogger(const JavaLogger &) = delete;
  JavaLogger &operator=(const JavaLogger &) = delete;

  ~JavaLogger() override {
    const ScopedEnv env(m_vm);
    if (env.get() == nullptr) {
      return; // JVM already gone; the refs die with it
    }
    if (m_sink != nullptr) {
      env.get()->DeleteGlobalRef(m_sink);
    }
    if (m_bridge != nullptr) {
      env.get()->DeleteGlobalRef(m_bridge);
    }
  }

  [[nodiscard]] bool will_log(const odr::LogLevel level) const override {
    const ScopedEnv env(m_vm);
    if (!usable(env.get())) {
      return false;
    }
    const auto result = env.get()->CallStaticBooleanMethod(
        m_bridge, m_will_log, m_sink, static_cast<jint>(level));
    return clear_pending(env.get()) ? false : result == JNI_TRUE;
  }

  void log(const Time time, const odr::LogLevel level,
           const std::string &message,
           const std::source_location &location) override {
    const ScopedEnv env(m_vm);
    if (!usable(env.get())) {
      return;
    }
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                            time.time_since_epoch())
                            .count();
    jstring file_name = to_jstring(env.get(), location.file_name());
    jstring function_name = to_jstring(env.get(), location.function_name());
    jstring text = to_jstring(env.get(), message);
    env.get()->CallStaticVoidMethod(
        m_bridge, m_log, m_sink, static_cast<jlong>(millis),
        static_cast<jint>(level), text, file_name, function_name,
        static_cast<jint>(location.line()));
    clear_pending(env.get());
  }

  void flush() override {
    const ScopedEnv env(m_vm);
    if (!usable(env.get())) {
      return;
    }
    env.get()->CallStaticVoidMethod(m_bridge, m_flush, m_sink);
    clear_pending(env.get());
  }

private:
  [[nodiscard]] bool usable(JNIEnv *env) const {
    return env != nullptr && m_sink != nullptr && m_bridge != nullptr &&
           m_will_log != nullptr && m_log != nullptr && m_flush != nullptr;
  }

  /// A logger must not derail the operation it is reporting on, so an exception
  /// thrown by the Java sink is swallowed rather than left pending.
  static bool clear_pending(JNIEnv *env) {
    if (env->ExceptionCheck() == JNI_FALSE) {
      return false;
    }
    env->ExceptionDescribe();
    env->ExceptionClear();
    return true;
  }

  JavaVM *m_vm{nullptr};
  jobject m_sink{nullptr};
  jclass m_bridge{nullptr};
  jmethodID m_will_log{nullptr};
  jmethodID m_log{nullptr};
  jmethodID m_flush{nullptr};
};

} // namespace

// app.opendocument.core.Logger

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Logger_createNull(JNIEnv *env, jclass) {
  return guarded(env, [&] { return make_handle(odr::Logger::null()); });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Logger_createStdio(JNIEnv *env, jclass, jstring name,
                                              jint level) {
  return guarded(env, [&] {
    return make_handle(odr::Logger::create_stdio(
        to_string(env, name), static_cast<odr::LogLevel>(level)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_app_opendocument_core_Logger_createFromSink(JNIEnv *env, jclass,
                                                 jobject sink) {
  return guarded(env, [&] {
    return make_handle(odr::Logger(std::make_shared<JavaLogger>(env, sink)));
  });
}

extern "C" JNIEXPORT jboolean JNICALL
Java_app_opendocument_core_Logger_willLogNative(JNIEnv *env, jobject,
                                                jlong handle, jint level) {
  return guarded(env, [&] {
    return static_cast<jboolean>(from_handle<odr::Logger>(handle)->will_log(
        static_cast<odr::LogLevel>(level)));
  });
}

extern "C" JNIEXPORT void JNICALL Java_app_opendocument_core_Logger_logNative(
    JNIEnv *env, jobject, jlong handle, jint level, jstring message) {
  guarded(env, [&] {
    from_handle<odr::Logger>(handle)->log(static_cast<odr::LogLevel>(level),
                                          to_string(env, message));
  });
}

extern "C" JNIEXPORT void JNICALL Java_app_opendocument_core_Logger_flushNative(
    JNIEnv *env, jobject, jlong handle) {
  guarded(env, [&] { from_handle<odr::Logger>(handle)->flush(); });
}

extern "C" JNIEXPORT void JNICALL
Java_app_opendocument_core_Logger_destroy(JNIEnv *, jclass, jlong handle) {
  destroy_handle<odr::Logger>(handle);
}
