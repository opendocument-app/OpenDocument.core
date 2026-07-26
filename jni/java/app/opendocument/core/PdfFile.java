package app.opendocument.core;

/** A decoded PDF file. Mirrors {@code odr::PdfFile}. */
public final class PdfFile extends DecodedFile {
  PdfFile(long handle) {
    super(handle);
  }

  /** Returns a decrypted copy of this file. */
  @Override
  public PdfFile decrypt(String password) {
    return new PdfFile(decryptPdfFileNative(handle(), password));
  }

  private native long decryptPdfFileNative(long handle, String password);
}
