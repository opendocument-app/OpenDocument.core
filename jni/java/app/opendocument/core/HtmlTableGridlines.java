package app.opendocument.core;

/** Mirrors {@code odr::HtmlTableGridlines}; constant order must match the C++ declaration. */
public enum HtmlTableGridlines {
  NONE, SOFT, HARD;

  static HtmlTableGridlines fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
