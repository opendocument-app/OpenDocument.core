package app.opendocument.core;

/** A decoded text file. Mirrors {@code odr::TextFile}. */
public final class TextFile extends DecodedFile {
  TextFile(long handle) {
    super(handle);
  }

  /** Detected character set; {@code null} if unknown. */
  public String charset() {
    return charsetNative(handle());
  }

  public String text() {
    return textNative(handle());
  }

  private native String charsetNative(long handle);

  private native String textNative(long handle);
}
