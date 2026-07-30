package app.opendocument.core;

/** A decoded document. Mirrors {@code odr::Document}. */
public final class Document extends NativeResource {
  Document(long handle) {
    super(handle, null, Document::destroy);
  }

  public boolean isEditable() {
    return isEditableNative(handle());
  }

  public boolean isSavable() {
    return isSavable(false);
  }

  public boolean isSavable(boolean encrypted) {
    return isSavableNative(handle(), encrypted);
  }

  public void save(String path) {
    saveNative(handle(), path);
  }

  public void save(String path, String password) {
    saveEncryptedNative(handle(), path, password);
  }

  public FileType fileType() {
    return FileType.fromNative(fileTypeNative(handle()));
  }

  public DocumentType documentType() {
    return DocumentType.fromNative(documentTypeNative(handle()));
  }

  /** Root of the element tree; keeps this document reachable. */
  public Element rootElement() {
    return new Element(rootElementNative(handle()), this);
  }

  public Filesystem asFilesystem() {
    return new Filesystem(asFilesystemNative(handle()), this);
  }

  /** Applies a diff; what {@link Html#edit} calls. */
  void edit(String diff) {
    editNative(handle(), diff);
  }

  private static native void destroy(long handle);

  private native void editNative(long handle, String diff);

  private native boolean isEditableNative(long handle);

  private native boolean isSavableNative(long handle, boolean encrypted);

  private native void saveNative(long handle, String path);

  private native void saveEncryptedNative(long handle, String path, String password);

  private native int fileTypeNative(long handle);

  private native int documentTypeNative(long handle);

  private native long rootElementNative(long handle);

  private native long asFilesystemNative(long handle);
}
