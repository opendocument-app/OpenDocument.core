package app.opendocument.core;

/** Frame element. Mirrors {@code odr::Frame}; geometry values may be {@code null}. */
public final class Frame extends Element {
  Frame(long handle, Object owner) {
    super(handle, owner);
  }

  public ShapeType shapeType() {
    return ShapeType.fromNative(shapeTypeNative(handle()));
  }

  public AnchorType anchorType() {
    return AnchorType.fromNative(anchorTypeNative(handle()));
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

  public Integer zIndex() {
    return zIndexNative(handle());
  }

  public DrawingTransform transform() {
    return transformNative(handle());
  }

  /** {@code null} for a shape whose geometry we cannot read, leaving its box. */
  public DrawingPath path() {
    return pathNative(handle());
  }

  /** The ends of a {@link ShapeType#LINE}, which states them instead of a box. */
  public DrawingLine line() {
    return lineNative(handle());
  }

  public GraphicStyle style() {
    return styleNative(handle());
  }

  private native int shapeTypeNative(long handle);

  private native int anchorTypeNative(long handle);

  private native Measure xNative(long handle);

  private native Measure yNative(long handle);

  private native Measure widthNative(long handle);

  private native Measure heightNative(long handle);

  private native Integer zIndexNative(long handle);

  private native DrawingTransform transformNative(long handle);

  private native DrawingPath pathNative(long handle);

  private native DrawingLine lineNative(long handle);

  private native GraphicStyle styleNative(long handle);
}
