package app.opendocument.core;

/** Paragraph element. Mirrors {@code odr::Paragraph}. */
public final class Paragraph extends Element {
  Paragraph(long handle, Object owner) {
    super(handle, owner);
  }

  public ParagraphStyle style() {
    return styleNative(handle());
  }

  public TextStyle textStyle() {
    return textStyleNative(handle());
  }

  private native ParagraphStyle styleNative(long handle);

  private native TextStyle textStyleNative(long handle);
}
