package app.opendocument.core;

/** Style of a table cell. Mirrors {@code odr::TableCellStyle}; fields may be {@code null}. */
public final class TableCellStyle {
  public final HorizontalAlign horizontalAlign;
  public final VerticalAlign verticalAlign;
  public final Color backgroundColor;
  public final DirectionalMeasure padding;
  public final DirectionalString border;
  public final Double textRotation;

  TableCellStyle(
      int horizontalAlign,
      int verticalAlign,
      Color backgroundColor,
      DirectionalMeasure padding,
      DirectionalString border,
      Double textRotation) {
    this.horizontalAlign = HorizontalAlign.fromNative(horizontalAlign);
    this.verticalAlign = VerticalAlign.fromNative(verticalAlign);
    this.backgroundColor = backgroundColor;
    this.padding = padding;
    this.border = border;
    this.textRotation = textRotation;
  }
}
