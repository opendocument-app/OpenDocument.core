package app.opendocument.core;

/** Dimensions of a table. Mirrors {@code odr::TableDimensions}. */
public final class TableDimensions {
  public int rows;
  public int columns;

  public TableDimensions() {}

  public TableDimensions(int rows, int columns) {
    this.rows = rows;
    this.columns = columns;
  }

  @Override
  public String toString() {
    return "TableDimensions(rows=" + rows + ", columns=" + columns + ")";
  }
}
