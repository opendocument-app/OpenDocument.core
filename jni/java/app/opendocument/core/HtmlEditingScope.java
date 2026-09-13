package app.opendocument.core;

/** Mirrors {@code odr::HtmlEditingScope}; constant order must match the C++ declaration. */
public enum HtmlEditingScope {
  RUN, DOCUMENT;

  static HtmlEditingScope fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
