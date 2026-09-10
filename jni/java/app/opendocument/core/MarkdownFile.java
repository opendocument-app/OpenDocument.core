package app.opendocument.core;

/** A decoded markdown file. Mirrors {@code odr::MarkdownFile}. */
public final class MarkdownFile extends DecodedFile {
  MarkdownFile(long handle) {
    super(handle);
  }

  /** The markdown as a text document. */
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
