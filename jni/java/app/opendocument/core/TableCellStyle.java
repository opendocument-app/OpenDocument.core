package app.opendocument.core;

/**
 * Style of a table cell. Mirrors {@code odr::TableCellStyle}; a {@code null} field is one the
 * document does not state. A caller builds one for {@link Sheet#setCellStyle}: every field left
 * {@code null} is left alone on the cell.
 */
public final class TableCellStyle {
  public HorizontalAlign horizontalAlign;
  public VerticalAlign verticalAlign;
  /** An alpha of 0 takes a fill away. */
  public Color backgroundColor;
  public DirectionalMeasure padding;
  public DirectionalString border;
  public Double textRotation;
  public Boolean wrapText;

  /** Every field {@code null}. */
  public TableCellStyle() {}

  TableCellStyle(
      int horizontalAlign,
      int verticalAlign,
      Color backgroundColor,
      DirectionalMeasure padding,
      DirectionalString border,
      Double textRotation,
      Boolean wrapText) {
    this.horizontalAlign = HorizontalAlign.fromNative(horizontalAlign);
    this.verticalAlign = VerticalAlign.fromNative(verticalAlign);
    this.backgroundColor = backgroundColor;
    this.padding = padding;
    this.border = border;
    this.textRotation = textRotation;
    this.wrapText = wrapText;
  }
}
