package app.opendocument.core;

/** Mirrors {@code odr::BreakType}; constant order must match the C++ declaration. */
public enum BreakType {
  NONE, PAGE, COLUMN;

  static BreakType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
