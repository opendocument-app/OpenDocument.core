package app.opendocument.core;

/** A decoded image file. Mirrors {@code odr::ImageFile}. */
public final class ImageFile extends DecodedFile {
  ImageFile(long handle) {
    super(handle);
  }

  /** Reads the image data into a byte array. */
  public byte[] read() {
    return readNative(handle());
  }

  private native byte[] readNative(long handle);
}
