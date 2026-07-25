package app.opendocument.core;

/** Custom shape element. Mirrors {@code odr::CustomShape}; x/y may be {@code null}. */
public final class CustomShape extends Element {
  CustomShape(long handle, Object owner) {
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
