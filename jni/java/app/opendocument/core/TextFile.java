package app.opendocument.core;

/** A decoded text file. Mirrors {@code odr::TextFile}. */
public final class TextFile extends DecodedFile {
  TextFile(long handle) {
    super(handle);
  }

  /** The encoding the bytes were detected as, or decoded with. */
  public TextEncoding encoding() {
    return TextEncoding.fromNative(encodingNative(handle()));
  }

  /**
   * Detected character set; {@code null} if unknown.
   *
   * @deprecated use {@link #encoding()}
   */
  @Deprecated
  public String charset() {
    TextEncoding encoding = encoding();
    return encoding == TextEncoding.UNKNOWN ? null : encoding.canonicalName();
  }

  public String text() {
    return textNative(handle());
  }

  private native int encodingNative(long handle);

  private native String textNative(long handle);
}
