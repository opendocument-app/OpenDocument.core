package app.opendocument.core;

/** Mirrors {@code odr::TableLayout}; constant order must match the C++ declaration. */
public enum TableLayout {
  AUTOMATIC, FIXED;

  static TableLayout fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
