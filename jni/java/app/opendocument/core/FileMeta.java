package app.opendocument.core;

/** Meta information about a file. Mirrors {@code odr::FileMeta}. */
public final class FileMeta {
  public final FileType type;
  public final String mimetype;
  public final boolean passwordEncrypted;

  /** {@code UNKNOWN} when the file is not a document; the fields below are then unset. */
  public final DocumentType documentType;

  /** Number of entries (pages/slides/sheets); {@code null} if unknown. */
  public final Long entryCount;

  public final String title;
  public final String author;
  public final String subject;
  public final String keywords;
  public final String creator;
  public final String producer;
  public final String creationDate;
  public final String modificationDate;

  FileMeta(
      int type,
      String mimetype,
      boolean passwordEncrypted,
      int documentType,
      Long entryCount,
      String title,
      String author,
      String subject,
      String keywords,
      String creator,
      String producer,
      String creationDate,
      String modificationDate) {
    this.type = FileType.fromNative(type);
    this.mimetype = mimetype;
    this.passwordEncrypted = passwordEncrypted;
    this.documentType = DocumentType.fromNative(documentType);
    this.entryCount = entryCount;
    this.title = title;
    this.author = author;
    this.subject = subject;
    this.keywords = keywords;
    this.creator = creator;
    this.producer = producer;
    this.creationDate = creationDate;
    this.modificationDate = modificationDate;
  }
}
