package app.opendocument.core;

/** A decoded document file. Mirrors {@code odr::DocumentFile}. */
public final class DocumentFile extends DecodedFile {
  DocumentFile(long handle) {
    super(handle);
  }

  public DocumentFile(String path) {
    this(create(path));
  }

  public static FileType typeByPath(String path) {
    return FileType.fromNative(typeByPathNative(path));
  }

  public static FileMeta metaByPath(String path) {
    return metaByPathNative(path);
  }

  public DocumentType documentType() {
    return DocumentType.fromNative(documentTypeNative(handle()));
  }

  /** Returns a decrypted copy of this file. */
  @Override
  public DocumentFile decrypt(String password) {
    return new DocumentFile(decryptDocumentFileNative(handle(), password));
  }

  public Document document() {
    return new Document(documentNative(handle()));
  }

  private static native long create(String path);

  private static native int typeByPathNative(String path);

  private static native FileMeta metaByPathNative(String path);

  private native int documentTypeNative(long handle);

  private native long decryptDocumentFileNative(long handle, String password);

  private native long documentNative(long handle);
}
