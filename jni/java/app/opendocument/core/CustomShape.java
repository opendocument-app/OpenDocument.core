package app.opendocument.core;

/**
 * Custom shape element. Mirrors {@code odr::CustomShape}; x/y may be {@code null}.
 *
 * @deprecated See {@link Rect}.
 */
@Deprecated
public final class CustomShape extends Element {
  CustomShape(long handle, Object owner) {
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

  public DrawingTransform transform() {
    return transformNative(handle());
  }

  public DrawingPath path() {
    return pathNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native Measure xNative(long handle);

  private native Measure yNative(long handle);

  private native Measure widthNative(long handle);

  private native Measure heightNative(long handle);

  private native DrawingTransform transformNative(long handle);

  private native DrawingPath pathNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
