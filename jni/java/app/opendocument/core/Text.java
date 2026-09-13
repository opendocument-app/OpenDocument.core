package app.opendocument.core;

/** Text element. Mirrors {@code odr::Text}. */
public final class Text extends Element {
  Text(long handle, Object owner) {
    super(handle, owner);
  }

  public String content() {
    return contentNative(handle());
  }

  public void setContent(String text) {
    setContentNative(handle(), text);
  }

  /**
   * States the non-null fields of {@code style} on the run and leaves the rest; a {@code
   * backgroundColor} with alpha 0 removes a highlight. {@code fontName}, {@code fontShadow} and
   * {@code fontPosition} are refused.
   */
  public void setStyle(TextStyle style) {
    setStyleNative(handle(), style);
  }

  public TextStyle style() {
    return styleNative(handle());
  }

  private native String contentNative(long handle);

  private native void setContentNative(long handle, String text);

  private native void setStyleNative(long handle, TextStyle style);

  private native TextStyle styleNative(long handle);
}
