package app.opendocument.core;

/** Mirrors {@code odr::GradientStyle}; constant order must match the C++ declaration. */
public enum GradientStyle {
  LINEAR, AXIAL, RADIAL, ELLIPSOID, SQUARE, RECTANGULAR;

  static GradientStyle fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
