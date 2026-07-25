package app.opendocument.core;

/** Line break element. Mirrors {@code odr::LineBreak}. */
public final class LineBreak extends Element {
  LineBreak(long handle, Object owner) {
    super(handle, owner);
  }

  public TextStyle style() {
    return styleNative(handle());
  }

  private native TextStyle styleNative(long handle);
}
