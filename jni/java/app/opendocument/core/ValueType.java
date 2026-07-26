package app.opendocument.core;

/** Mirrors {@code odr::ValueType}; constant order must match the C++ declaration. */
public enum ValueType {
  UNKNOWN, STRING, FLOAT_NUMBER;

  static ValueType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
