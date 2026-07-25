package app.opendocument.core;

/** Image element. Mirrors {@code odr::Image}. */
public final class Image extends Element {
  Image(long handle, Object owner) {
    super(handle, owner);
  }

  public boolean isInternal() {
    return isInternalNative(handle());
  }

  /** The image file for internal images; {@code null} otherwise. */
  public File file() {
    long h = fileNative(handle());
    return h == 0 ? null : new File(h);
  }

  public String href() {
    return hrefNative(handle());
  }

  private native boolean isInternalNative(long handle);

  private native long fileNative(long handle);

  private native String hrefNative(long handle);
}
