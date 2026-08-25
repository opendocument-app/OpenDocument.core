package app.opendocument.core;

/** Mirrors {@code odr::HtmlViewportMode}; constant order must match the C++ declaration. */
public enum HtmlViewportMode {
  AUTOMATIC, FIT_WIDTH, ACTUAL_SIZE, NONE, FIT_WIDTH_BY_VIEW;

  static HtmlViewportMode fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
