package app.opendocument.core;

/** Mirrors {@code odr::FontStyle}; constant order must match the C++ declaration. */
public enum FontStyle {
  NORMAL, ITALIC;

  static FontStyle fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
