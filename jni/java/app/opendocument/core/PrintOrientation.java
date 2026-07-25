package app.opendocument.core;

/** Mirrors {@code odr::PrintOrientation}; constant order must match the C++ declaration. */
public enum PrintOrientation {
  PORTRAIT, LANDSCAPE;

  static PrintOrientation fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
