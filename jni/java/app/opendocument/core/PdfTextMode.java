package app.opendocument.core;

/** Mirrors {@code odr::PdfTextMode}; constant order must match the C++ declaration. */
public enum PdfTextMode {
  DUAL_LAYER, SINGLE_LAYER;

  static PdfTextMode fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
