package app.opendocument.core;

/**
 * Entry points the native side calls to reach an {@link ILogger}. Keeping the
 * enum and {@link SourceLocation} construction on this side means the native
 * bridge only has to cache three static method handles.
 */
final class LoggerBridge {
  private LoggerBridge() {}

  static boolean willLog(ILogger sink, int level) {
    return sink.willLog(LogLevel.fromNative(level));
  }

  static void log(
      ILogger sink,
      long epochMillis,
      int level,
      String message,
      String fileName,
      String functionName,
      int line) {
    sink.log(
        epochMillis,
        LogLevel.fromNative(level),
        message,
        new SourceLocation(fileName, functionName, line));
  }

  static void flush(ILogger sink) {
    sink.flush();
  }
}
