package app.opendocument.core;

/** Mirrors {@code odr::LogLevel}; constant order must match the C++ declaration. */
public enum LogLevel {
  VERBOSE, DEBUG, INFO, WARNING, ERROR, FATAL;

  static LogLevel fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
