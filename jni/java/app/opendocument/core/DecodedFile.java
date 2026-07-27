package app.opendocument.core;

/**
 * A decoded file. Mirrors {@code odr::DecodedFile}. Obtain via
 * {@link Odr#open} or the constructors; {@code as*} accessors return typed
 * views and throw when the file is not of that kind (check {@code is*} first).
 */
public class DecodedFile extends NativeResource {
  static {
    NativeLibrary.load();
  }

  DecodedFile(long handle) {
    super(handle, null, DecodedFile::destroy);
  }

  public DecodedFile(String path) {
    this(create(path));
  }

  public DecodedFile(String path, FileType as) {
    this(createAs(path, as.toNative()));
  }

  public DecodedFile(File file) {
    this(createFromFile(file.handle()));
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

  public boolean isDecodable() {
    return isDecodableNative(handle());
  }

  public boolean isTextFile() {
    return isTextFileNative(handle());
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

  private static native long create(String path);

  private static native long createAs(String path, int as);

  private static native long createFromFile(long fileHandle);

  static native void destroy(long handle);

  private native long fileNative(long handle);

  private native int fileTypeNative(long handle);

  private native int fileCategoryNative(long handle);

  private native FileMeta fileMetaNative(long handle);

  private native boolean passwordEncryptedNative(long handle);

  private native int encryptionStateNative(long handle);

  private native long decryptNative(long handle, String password);

  private native boolean isDecodableNative(long handle);

  private native boolean isTextFileNative(long handle);

  private native boolean isImageFileNative(long handle);

  private native boolean isArchiveFileNative(long handle);

  private native boolean isDocumentFileNative(long handle);

  private native boolean isPdfFileNative(long handle);

  private native boolean isFontFileNative(long handle);

  private native long asTextFileNative(long handle);

  private native long asImageFileNative(long handle);

  private native long asArchiveFileNative(long handle);

  private native long asDocumentFileNative(long handle);

  private native long asPdfFileNative(long handle);

  private native long asFontFileNative(long handle);
}
