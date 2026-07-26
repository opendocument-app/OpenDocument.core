package app.opendocument.core;

/** Line element. Mirrors {@code odr::Line}. */
public final class Line extends Element {
  Line(long handle, Object owner) {
    super(handle, owner);
  }

  public String x1() {
    return x1Native(handle());
  }

  public String y1() {
    return y1Native(handle());
  }

  public String x2() {
    return x2Native(handle());
  }

  public String y2() {
    return y2Native(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native String x1Native(long handle);

  private native String y1Native(long handle);

  private native String x2Native(long handle);

  private native String y2Native(long handle);

  private native GraphicStyle styleNative(long handle);
}
