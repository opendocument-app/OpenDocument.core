package app.opendocument.core;

/** An archive (e.g. ZIP contents). Mirrors {@code odr::Archive}. */
public final class Archive extends NativeResource {
  Archive(long handle) {
    super(handle, null, Archive::destroy);
  }

  public Filesystem asFilesystem() {
    return new Filesystem(asFilesystemNative(handle()), null);
  }

  /** Serializes the archive into a byte array. */
  public byte[] save() {
    return saveNative(handle());
  }

  private static native void destroy(long handle);

  private native long asFilesystemNative(long handle);

  private native byte[] saveNative(long handle);
}
