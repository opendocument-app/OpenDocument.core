package app.opendocument.core;

/** A text document. Mirrors {@code odr::TextDocument}. */
public final class TextDocument extends Document {
  TextDocument(long handle) {
    super(handle);
  }
}
