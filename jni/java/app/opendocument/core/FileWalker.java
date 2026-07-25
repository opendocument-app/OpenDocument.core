package app.opendocument.core;

/** Walks the entries of a {@link Filesystem}. Mirrors {@code odr::FileWalker}. */
public final class FileWalker extends NativeResource {
  FileWalker(long handle, Object owner) {
    super(handle, owner, FileWalker::destroy);
  }

  public boolean end() {
    return endNative(handle());
  }

  public int depth() {
    return depthNative(handle());
  }

  public String path() {
    return pathNative(handle());
  }

  public boolean isFile() {
    return isFileNative(handle());
  }

  public boolean isDirectory() {
    return isDirectoryNative(handle());
  }

  public void pop() {
    popNative(handle());
  }

  public void next() {
    nextNative(handle());
  }

  public void flatNext() {
    flatNextNative(handle());
  }

  private static native void destroy(long handle);

  private native boolean endNative(long handle);

  private native int depthNative(long handle);

  private native String pathNative(long handle);

  private native boolean isFileNative(long handle);

  private native boolean isDirectoryNative(long handle);

  private native void popNative(long handle);

  private native void nextNative(long handle);

  private native void flatNextNative(long handle);
}
