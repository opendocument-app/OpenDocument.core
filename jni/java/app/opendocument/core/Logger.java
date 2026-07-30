package app.opendocument.core;

/**
 * Handle to a log sink. Mirrors {@code odr::Logger}; copies share the sink.
 *
 * <p>Pass one to {@link Odr#open} (and friends) to receive the library's
 * diagnostics. Wrap your own {@link ILogger} to route them anywhere.
 */
public final class Logger extends NativeResource {
  static {
    NativeLibrary.load();
  }

  Logger(long handle) {
    super(handle, null, Logger::destroy);
  }

  /** A logger that discards everything. */
  public static Logger nullLogger() {
    return new Logger(createNull());
  }

  /** Writes to standard output. */
  public static Logger stdio(String name, LogLevel level) {
    return new Logger(createStdio(name, level.toNative()));
  }

  /** Routes to a sink implemented in Java. */
  public Logger(ILogger sink) {
    this(createFromSink(sink));
  }

  public boolean willLog(LogLevel level) {
    return willLogNative(handle(), level.toNative());
  }

  public void log(LogLevel level, String message) {
    logNative(handle(), level.toNative(), message);
  }

  public void flush() {
    flushNative(handle());
  }

  private static native long createNull();

  private static native long createStdio(String name, int level);

  private static native long createFromSink(ILogger sink);

  private native boolean willLogNative(long handle, int level);

  private native void logNative(long handle, int level, String message);

  private native void flushNative(long handle);

  private static native void destroy(long handle);
}
