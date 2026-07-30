package app.opendocument.core;

/** A path to a specific element in a document. Mirrors {@code odr::DocumentPath}. */
public final class DocumentPath extends NativeResource {
  static {
    NativeLibrary.load();
  }

  DocumentPath(long handle) {
    super(handle, null, DocumentPath::destroy);
  }

  public DocumentPath(String path) {
    this(create(path));
  }

  public boolean empty() {
    return emptyNative(handle());
  }

  public DocumentPath parent() {
    return new DocumentPath(parentNative(handle()));
  }

  public DocumentPath join(DocumentPath other) {
    try {
      return new DocumentPath(joinNative(handle(), other.handle()));
    } finally {
      other.keepAlive();
    }
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof DocumentPath path && toString().equals(path.toString());
  }

  @Override
  public int hashCode() {
    return toString().hashCode();
  }

  @Override
  public String toString() {
    return toStringNative(handle());
  }

  private static native long create(String path);

  private static native void destroy(long handle);

  private native boolean emptyNative(long handle);

  private native long parentNative(long handle);

  private native long joinNative(long handle, long otherHandle);

  private native String toStringNative(long handle);
}
