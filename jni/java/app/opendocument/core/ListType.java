package app.opendocument.core;

/** Mirrors {@code odr::ListType}; constant order must match the C++ declaration. */
public enum ListType {
  UNORDERED, ORDERED;

  static ListType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
