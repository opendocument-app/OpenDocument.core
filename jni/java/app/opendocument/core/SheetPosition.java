package app.opendocument.core;

/** A cell of a sheet: the sheet's ordinal, the column and the row. Mirrors {@code odr::SheetPosition}. */
public final class SheetPosition {
  private final int sheet;
  private final int column;
  private final int row;

  public SheetPosition(int sheet, int column, int row) {
    this.sheet = sheet;
    this.column = column;
    this.row = row;
  }

  public int sheet() {
    return sheet;
  }

  public int column() {
    return column;
  }

  public int row() {
    return row;
  }

  @Override
  public boolean equals(Object other) {
    if (!(other instanceof SheetPosition)) {
      return false;
    }
    SheetPosition position = (SheetPosition) other;
    return sheet == position.sheet && column == position.column && row == position.row;
  }

  @Override
  public int hashCode() {
    return (sheet * 31 + column) * 31 + row;
  }

  @Override
  public String toString() {
    return "SheetPosition(sheet=" + sheet + ", column=" + column + ", row=" + row + ")";
  }
}
