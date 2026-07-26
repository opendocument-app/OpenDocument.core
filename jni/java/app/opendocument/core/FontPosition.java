package app.opendocument.core;

/** Mirrors {@code odr::FontPosition}; constant order must match the C++ declaration. */
public enum FontPosition {
  NORMAL, SUPER, SUB;

  static FontPosition fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
