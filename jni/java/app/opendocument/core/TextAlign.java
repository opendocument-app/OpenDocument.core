package app.opendocument.core;

/** Mirrors {@code odr::TextAlign}; constant order must match the C++ declaration. */
public enum TextAlign {
  LEFT, RIGHT, CENTER, JUSTIFY, START, END;

  static TextAlign fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
