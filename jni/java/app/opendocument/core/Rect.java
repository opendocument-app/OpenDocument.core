package app.opendocument.core;

/** Rectangle element. Mirrors {@code odr::Rect}. */
public final class Rect extends Element {
  Rect(long handle, Object owner) {
    super(handle, owner);
  }

  public Measure x() {
    return xNative(handle());
  }

  public Measure y() {
    return yNative(handle());
  }

  public Measure width() {
    return widthNative(handle());
  }

  public Measure height() {
    return heightNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native Measure xNative(long handle);

  private native Measure yNative(long handle);

  private native Measure widthNative(long handle);

  private native Measure heightNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
