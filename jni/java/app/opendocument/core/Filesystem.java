package app.opendocument.core;

/** A readable filesystem. Mirrors {@code odr::Filesystem}. */
public final class Filesystem extends NativeResource {
  Filesystem(long handle, Object owner) {
    super(handle, owner, Filesystem::destroy);
  }

  public boolean exists(String path) {
    return existsNative(handle(), path);
  }

  public boolean isFile(String path) {
    return isFileNative(handle(), path);
  }

  public boolean isDirectory(String path) {
    return isDirectoryNative(handle(), path);
  }

  public FileWalker fileWalker(String path) {
    return new FileWalker(fileWalkerNative(handle(), path), this);
  }

  public File open(String path) {
    return new File(openNative(handle(), path));
  }

  private static native void destroy(long handle);

  private native boolean existsNative(long handle, String path);

  private native boolean isFileNative(long handle, String path);

  private native boolean isDirectoryNative(long handle, String path);

  private native long fileWalkerNative(long handle, String path);

  private native long openNative(long handle, String path);
}
