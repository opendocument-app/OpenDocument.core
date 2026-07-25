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

  public TextStyle style() {
    return styleNative(handle());
  }

  private native String contentNative(long handle);

  private native void setContentNative(long handle, String text);

  private native TextStyle styleNative(long handle);
}
