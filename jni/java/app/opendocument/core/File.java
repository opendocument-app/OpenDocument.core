package app.opendocument.core;

/** An undecoded file. Mirrors {@code odr::File}. */
public final class File extends NativeResource {
  static {
    NativeLibrary.load();
  }

  File(long handle) {
    super(handle, null, File::destroy);
  }

  public File(String path) {
    this(create(path));
  }

  public FileLocation location() {
    return FileLocation.fromNative(locationNative(handle()));
  }

  public long size() {
    return sizeNative(handle());
  }

  /** The file name, without any directory; empty where there is none. */
  public String name() {
    return nameNative(handle());
  }

  /** Path on disk; {@code null} for in-memory files. */
  public String diskPath() {
    return diskPathNative(handle());
  }

  /** Reads the whole file into a byte array. */
  public byte[] read() {
    return readNative(handle());
  }

  public void copy(String path) {
    copyNative(handle(), path);
  }

  /** Decodes this file; the handle for {@link DecodedFile#DecodedFile(File)}. */
  long decode() {
    return decodeNative(handle());
  }

  private static native long create(String path);

  private static native void destroy(long handle);

  private native long decodeNative(long handle);

  private native int locationNative(long handle);

  private native long sizeNative(long handle);

  private native String nameNative(long handle);

  private native String diskPathNative(long handle);

  private native byte[] readNative(long handle);

  private native void copyNative(long handle, String path);
}
