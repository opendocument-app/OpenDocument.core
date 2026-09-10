package app.opendocument.core;

/**
 * A decoded file. Mirrors {@code odr::DecodedFile}. Obtain via {@link Odr#open};
 * {@code as*} accessors return typed views and throw when the file is not of
 * that kind (check {@code is*} first).
 */
public class DecodedFile extends NativeResource {
  static {
    NativeLibrary.load();
  }

  DecodedFile(long handle) {
    super(handle, null, DecodedFile::destroy);
  }

  public File file() {
    return new File(fileNative(handle()));
  }

  public FileType fileType() {
    return FileType.fromNative(fileTypeNative(handle()));
  }

  public FileCategory fileCategory() {
    return FileCategory.fromNative(fileCategoryNative(handle()));
  }

  public FileMeta fileMeta() {
    return fileMetaNative(handle());
  }

  public boolean passwordEncrypted() {
    return passwordEncryptedNative(handle());
  }

  public EncryptionState encryptionState() {
    return EncryptionState.fromNative(encryptionStateNative(handle()));
  }

  /** Returns a decrypted copy of this file. */
  public DecodedFile decrypt(String password) {
    return new DecodedFile(decryptNative(handle(), password));
  }

  /**
   * What can be done with this file. Refines {@link Odr#capabilitiesByFileType}; {@code edit},
   * {@code save} and {@code encrypt} stay as declared for the format — ask {@link Document} for
   * the precise answer.
   */
  public FileTypeCapabilities capabilities() {
    return capabilitiesNative(handle());
  }

  public boolean isDecodable() {
    return isDecodableNative(handle());
  }

  public boolean isTextFile() {
    return isTextFileNative(handle());
  }

  /** A csv holds a text file rather than being one; {@link #isTextFile} is false. */
  public boolean isCsvFile() {
    return isCsvFileNative(handle());
  }

  /** Markdown holds a text file the same way a csv does. */
  public boolean isMarkdownFile() {
    return isMarkdownFileNative(handle());
  }

  public boolean isImageFile() {
    return isImageFileNative(handle());
  }

  public boolean isArchiveFile() {
    return isArchiveFileNative(handle());
  }

  public boolean isDocumentFile() {
    return isDocumentFileNative(handle());
  }

  public boolean isPdfFile() {
    return isPdfFileNative(handle());
  }

  public boolean isFontFile() {
    return isFontFileNative(handle());
  }

  public TextFile asTextFile() {
    return new TextFile(asTextFileNative(handle()));
  }

  public CsvFile asCsvFile() {
    return new CsvFile(asCsvFileNative(handle()));
  }

  public MarkdownFile asMarkdownFile() {
    return new MarkdownFile(asMarkdownFileNative(handle()));
  }

  public ImageFile asImageFile() {
    return new ImageFile(asImageFileNative(handle()));
  }

  public ArchiveFile asArchiveFile() {
    return new ArchiveFile(asArchiveFileNative(handle()));
  }

  public DocumentFile asDocumentFile() {
    return new DocumentFile(asDocumentFileNative(handle()));
  }

  public PdfFile asPdfFile() {
    return new PdfFile(asPdfFileNative(handle()));
  }

  public FontFile asFontFile() {
    return new FontFile(asFontFileNative(handle()));
  }

  static native void destroy(long handle);

  private native long fileNative(long handle);

  private native int fileTypeNative(long handle);

  private native int fileCategoryNative(long handle);

  private native FileMeta fileMetaNative(long handle);

  private native boolean passwordEncryptedNative(long handle);

  private native int encryptionStateNative(long handle);

  private native long decryptNative(long handle, String password);

  private native FileTypeCapabilities capabilitiesNative(long handle);

  private native boolean isDecodableNative(long handle);

  private native boolean isTextFileNative(long handle);

  private native boolean isCsvFileNative(long handle);

  private native boolean isMarkdownFileNative(long handle);

  private native boolean isImageFileNative(long handle);

  private native boolean isArchiveFileNative(long handle);

  private native boolean isDocumentFileNative(long handle);

  private native boolean isPdfFileNative(long handle);

  private native boolean isFontFileNative(long handle);

  private native long asTextFileNative(long handle);

  private native long asCsvFileNative(long handle);

  private native long asMarkdownFileNative(long handle);

  private native long asImageFileNative(long handle);

  private native long asArchiveFileNative(long handle);

  private native long asDocumentFileNative(long handle);

  private native long asPdfFileNative(long handle);

  private native long asFontFileNative(long handle);
}
