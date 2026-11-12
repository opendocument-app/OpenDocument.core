package app.opendocument.core;

/** Mirrors {@code odr::TextPathMode}; constant order must match the C++ declaration. */
public enum TextPathMode {
  NORMAL, PATH, SHAPE;

  static TextPathMode fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
