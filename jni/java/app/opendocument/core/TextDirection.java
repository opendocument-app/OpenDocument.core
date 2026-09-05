package app.opendocument.core;

/** Mirrors {@code odr::TextDirection}; constant order must match the C++ declaration. */
public enum TextDirection {
  LEFT_TO_RIGHT, RIGHT_TO_LEFT;

  static TextDirection fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
