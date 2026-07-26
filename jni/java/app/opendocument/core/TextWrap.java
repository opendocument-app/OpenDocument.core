package app.opendocument.core;

/** Mirrors {@code odr::TextWrap}; constant order must match the C++ declaration. */
public enum TextWrap {
  NONE, BEFORE, AFTER, RUN_THROUGH;

  static TextWrap fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
