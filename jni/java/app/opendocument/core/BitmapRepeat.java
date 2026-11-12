package app.opendocument.core;

/** Mirrors {@code odr::BitmapRepeat}; constant order must match the C++ declaration. */
public enum BitmapRepeat {
  REPEAT, STRETCH, NO_REPEAT;

  static BitmapRepeat fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
