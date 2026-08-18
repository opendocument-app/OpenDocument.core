package app.opendocument.core;

/** Mirrors {@code odr::HtmlColorScheme}; constant order must match the C++ declaration. */
public enum HtmlColorScheme {
  LIGHT, DARK, SYSTEM;

  static HtmlColorScheme fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
