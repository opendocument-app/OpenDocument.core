package app.opendocument.core;

/** Style of a table row. Mirrors {@code odr::TableRowStyle}; fields may be {@code null}. */
public final class TableRowStyle {
  public final Measure height;

  TableRowStyle(Measure height) {
    this.height = height;
  }
}
