package app.opendocument.core;

/** Line element. Mirrors {@code odr::Line}. */
public final class Line extends Element {
  Line(long handle, Object owner) {
    super(handle, owner);
  }

  public Measure x1() {
    return x1Native(handle());
  }

  public Measure y1() {
    return y1Native(handle());
  }

  public Measure x2() {
    return x2Native(handle());
  }

  public Measure y2() {
    return y2Native(handle());
  }

  public DrawingTransform transform() {
    return transformNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native Measure x1Native(long handle);

  private native Measure y1Native(long handle);

  private native Measure x2Native(long handle);

  private native Measure y2Native(long handle);

  private native DrawingTransform transformNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
