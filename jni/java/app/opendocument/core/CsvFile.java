package app.opendocument.core;

/** A decoded csv. Mirrors {@code odr::CsvFile}. */
public final class CsvFile extends DecodedFile {
  CsvFile(long handle) {
    super(handle);
  }

  /** The csv as a one-sheet spreadsheet. */
  public Document document() {
    return new Document(documentNative(handle()));
  }

  /** The same bytes as plain text, so reading them needs no reopening. */
  public TextFile textFile() {
    return new TextFile(textFileNative(handle()));
  }

  private native long documentNative(long handle);

  private native long textFileNative(long handle);
}
