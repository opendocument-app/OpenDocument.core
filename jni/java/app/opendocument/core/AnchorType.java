package app.opendocument.core;

/** Mirrors {@code odr::AnchorType}; constant order must match the C++ declaration. */
public enum AnchorType {
  /** The frame does not exist, so it is anchored nowhere. */
  NONE,
  AS_CHAR,
  AT_CHAR,
  AT_FRAME,
  AT_PAGE,
  AT_PARAGRAPH;

  static AnchorType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
