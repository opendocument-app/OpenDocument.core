package app.opendocument.core;

/** Mirrors {@code odr::HorizontalAlign}; constant order must match the C++ declaration. */
public enum HorizontalAlign {
  LEFT, CENTER, RIGHT;

  static HorizontalAlign fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
