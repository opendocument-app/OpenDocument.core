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

  /**
   * False where the file type is one this library does not write, or the
   * encoding cannot be decoded.
   */
  public boolean isSavable() {
    return isSavableNative(handle());
  }

  /**
   * Applies the operations and returns the result, as UTF-8 whatever the source
   * encoding was.
   */
  public byte[] writeEdited(String operations) {
    return writeEditedNative(handle(), operations);
  }

  private native int encodingNative(long handle);

  private native boolean isSavableNative(long handle);

  private native byte[] writeEditedNative(long handle, String operations);

  private native String textNative(long handle);
}
