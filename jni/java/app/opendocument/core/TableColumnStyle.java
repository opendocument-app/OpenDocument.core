package app.opendocument.core;

/** Style of a table column. Mirrors {@code odr::TableColumnStyle}; fields may be {@code null}. */
public final class TableColumnStyle {
  public final Measure width;

  TableColumnStyle(Measure width) {
    this.width = width;
  }
}
