package app.opendocument.core;

/**
 * Rectangle element. Mirrors {@code odr::Rect}.
 *
 * @deprecated Merging into {@link Frame}, which gains a shape kind.
 */
@Deprecated
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

  public DrawingTransform transform() {
    return transformNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native Measure xNative(long handle);

  private native Measure yNative(long handle);

  private native Measure widthNative(long handle);

  private native Measure heightNative(long handle);

  private native DrawingTransform transformNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
