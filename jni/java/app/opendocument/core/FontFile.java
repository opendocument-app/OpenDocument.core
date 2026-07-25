package app.opendocument.core;

/** A decoded font file. Mirrors {@code odr::FontFile}. */
public final class FontFile extends DecodedFile {
  FontFile(long handle) {
    super(handle);
  }

  /** Reads the font data into a byte array. */
  public byte[] read() {
    return readNative(handle());
  }

  private native byte[] readNative(long handle);
}
