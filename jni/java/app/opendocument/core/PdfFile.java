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

  /**
   * Applies markup annotations and returns the annotated pdf.
   *
   * @param annotations the payload the rendered page's {@code
   *     odr.annotation.getAnnotations()} collects.
   */
  public byte[] annotate(String annotations) {
    return annotateNative(handle(), annotations);
  }

  private native long decryptPdfFileNative(long handle, String password);

  private native byte[] annotateNative(long handle, String annotations);
}
