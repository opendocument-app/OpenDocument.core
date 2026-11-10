package app.opendocument.core;

/** Mirrors {@code odr::HatchStyle}; constant order must match the C++ declaration. */
public enum HatchStyle {
  SINGLE, CROSSED, TRIPLE;

  static HatchStyle fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
