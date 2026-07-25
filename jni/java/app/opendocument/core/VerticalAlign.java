package app.opendocument.core;

/** Mirrors {@code odr::VerticalAlign}; constant order must match the C++ declaration. */
public enum VerticalAlign {
  TOP, MIDDLE, BOTTOM;

  static VerticalAlign fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
