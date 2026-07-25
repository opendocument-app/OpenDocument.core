package app.opendocument.core;

/** A resource referenced by rendered HTML. Mirrors {@code odr::HtmlResource}. */
public final class HtmlResource extends NativeResource {
  HtmlResource(long handle) {
    super(handle, null, HtmlResource::destroy);
  }

  public HtmlResourceType type() {
    return HtmlResourceType.fromNative(typeNative(handle()));
  }

  public String mimeType() {
    return mimeTypeNative(handle());
  }

  public String name() {
    return nameNative(handle());
  }

  public String path() {
    return pathNative(handle());
  }

  /** The backing file; {@code null} if not file-backed. */
  public File file() {
    long h = fileNative(handle());
    return h == 0 ? null : new File(h);
  }

  public boolean isShipped() {
    return isShippedNative(handle());
  }

  public boolean isExternal() {
    return isExternalNative(handle());
  }

  public boolean isAccessible() {
    return isAccessibleNative(handle());
  }

  /** Reads the resource content into a byte array. */
  public byte[] read() {
    return readNative(handle());
  }

  private static native void destroy(long handle);

  private native int typeNative(long handle);

  private native String mimeTypeNative(long handle);

  private native String nameNative(long handle);

  private native String pathNative(long handle);

  private native long fileNative(long handle);

  private native boolean isShippedNative(long handle);

  private native boolean isExternalNative(long handle);

  private native boolean isAccessibleNative(long handle);

  private native byte[] readNative(long handle);
}
