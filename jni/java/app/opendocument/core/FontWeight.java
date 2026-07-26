package app.opendocument.core;

/** Mirrors {@code odr::FontWeight}; constant order must match the C++ declaration. */
public enum FontWeight {
  NORMAL, BOLD;

  static FontWeight fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
