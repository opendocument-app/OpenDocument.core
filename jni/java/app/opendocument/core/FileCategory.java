package app.opendocument.core;

/** Mirrors {@code odr::FileCategory}; constant order must match the C++ declaration. */
public enum FileCategory {
  UNKNOWN, TEXT, IMAGE, ARCHIVE, DOCUMENT, FONT, AUDIO, VIDEO;

  static FileCategory fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
