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
   * Applies the operations our browser-side editor produces. The text is UTF-8 afterwards,
   * whatever the source encoding was.
   */
  public void edit(String operations) {
    editNative(handle(), operations);
  }

  /** Writes the text, with every edit applied, as UTF-8. */
  public void save(String path) {
    saveNative(handle(), path);
  }

  /** The saved file as bytes. */
  public byte[] saveToMemory() {
    return saveToMemoryNative(handle());
  }

  /**
   * Applies the operations and returns the result, as UTF-8 whatever the source encoding was. The
   * file stays as it is.
   *
   * @deprecated use {@link #edit(String)}, then {@link #saveToMemory()}
   */
  @Deprecated
  public byte[] writeEdited(String operations) {
    return writeEditedNative(handle(), operations);
  }

  private native int encodingNative(long handle);

  private native void editNative(long handle, String operations);

  private native void saveNative(long handle, String path);

  private native byte[] saveToMemoryNative(long handle);

  private native boolean isSavableNative(long handle);

  private native byte[] writeEditedNative(long handle, String operations);

  private native String textNative(long handle);
}
