package app.opendocument.core;

/** Style of a table. Mirrors {@code odr::TableStyle}; fields may be {@code null}. */
public final class TableStyle {
  public final Measure width;

  TableStyle(Measure width) {
    this.width = width;
  }
}
