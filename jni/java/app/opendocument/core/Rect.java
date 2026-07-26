package app.opendocument.core;

/** Rectangle element. Mirrors {@code odr::Rect}. */
public final class Rect extends Element {
  Rect(long handle, Object owner) {
    super(handle, owner);
  }

  public String x() {
    return xNative(handle());
  }

  public String y() {
    return yNative(handle());
  }

  public String width() {
    return widthNative(handle());
  }

  public String height() {
    return heightNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native String xNative(long handle);

  private native String yNative(long handle);

  private native String widthNative(long handle);

  private native String heightNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
