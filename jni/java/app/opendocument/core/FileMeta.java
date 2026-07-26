package app.opendocument.core;

/** Meta information about a file. Mirrors {@code odr::FileMeta}. */
public final class FileMeta {
  public final FileType type;
  public final String mimetype;
  public final boolean passwordEncrypted;
  /** {@code null} when the file is not a document. */
  public final DocumentMeta documentMeta;

  FileMeta(int type, String mimetype, boolean passwordEncrypted, DocumentMeta documentMeta) {
    this.type = FileType.fromNative(type);
    this.mimetype = mimetype;
    this.passwordEncrypted = passwordEncrypted;
    this.documentMeta = documentMeta;
  }
}
