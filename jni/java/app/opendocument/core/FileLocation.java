package app.opendocument.core;

/** Mirrors {@code odr::FileLocation}; constant order must match the C++ declaration. */
public enum FileLocation {
  UNKNOWN, MEMORY, DISK;

  static FileLocation fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
