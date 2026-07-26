package app.opendocument.core;

/** Mirrors {@code odr::DocumentType}; constant order must match the C++ declaration. */
public enum DocumentType {
  UNKNOWN, TEXT, PRESENTATION, SPREADSHEET, DRAWING;

  static DocumentType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
