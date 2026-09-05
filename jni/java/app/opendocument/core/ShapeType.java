package app.opendocument.core;

/** Mirrors {@code odr::ShapeType}; constant order must match the C++ declaration. */
public enum ShapeType {
  NONE, RECT, ELLIPSE, LINE, CUSTOM;

  static ShapeType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
