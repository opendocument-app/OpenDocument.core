package app.opendocument.core;

/** Mirrors {@code odr::HtmlResourceType}; constant order must match the C++ declaration. */
public enum HtmlResourceType {
  HTML_FRAGMENT, CSS, JS, IMAGE, FONT, MEDIA;

  static HtmlResourceType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
