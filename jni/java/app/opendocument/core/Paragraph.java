package app.opendocument.core;

/** Paragraph element. Mirrors {@code odr::Paragraph}. */
public final class Paragraph extends Element {
  Paragraph(long handle, Object owner) {
    super(handle, owner);
  }

  public ParagraphStyle style() {
    return styleNative(handle());
  }

  /**
   * States the non-null fields of {@code style} on the paragraph and leaves the rest. Only {@code
   * textAlign} is written; any other field is refused.
   */
  public void setStyle(ParagraphStyle style) {
    setStyleNative(handle(), style);
  }

  public TextStyle textStyle() {
    return textStyleNative(handle());
  }

  private native ParagraphStyle styleNative(long handle);

  private native void setStyleNative(long handle, ParagraphStyle style);

  private native TextStyle textStyleNative(long handle);
}
