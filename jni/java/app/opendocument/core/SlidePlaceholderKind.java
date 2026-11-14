package app.opendocument.core;

/**
 * Mirrors {@code odr::SlidePlaceholderKind}; constant order must match the C++ declaration.
 */
public enum SlidePlaceholderKind {
  TITLE, SUBTITLE, BODY;

  static SlidePlaceholderKind fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
