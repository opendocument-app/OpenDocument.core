package app.opendocument.core;

/** Mirrors {@code odr::HtmlPageLayout}; constant order must match the C++ declaration. */
public enum HtmlPageLayout {
  CONTINUOUS, SINGLE;

  static HtmlPageLayout fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
