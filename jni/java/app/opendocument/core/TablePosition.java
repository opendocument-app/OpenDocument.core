package app.opendocument.core;

/** A cell position in a table. Mirrors {@code odr::TablePosition}. */
public final class TablePosition {
  static {
    NativeLibrary.load();
  }

  public int column;
  public int row;

  public TablePosition() {}

  public TablePosition(int column, int row) {
    this.column = column;
    this.row = row;
  }

  /** Parses a column letter (e.g. {@code "A"}) to a 0-based column number. */
  public static native int toColumnNum(String string);

  /** Parses a row label (e.g. {@code "1"}) to a 0-based row number. */
  public static native int toRowNum(String string);

  /** Formats a 0-based column number as a column letter. */
  public static native String toColumnString(int column);

  /** Formats a 0-based row number as a row label. */
  public static native String toRowString(int row);

  @Override
  public String toString() {
    return toColumnString(column) + toRowString(row);
  }
}
