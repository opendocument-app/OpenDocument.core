package app.opendocument.core;

/** A spreadsheet: its sheets. Mirrors {@code odr::Spreadsheet}. */
public final class Spreadsheet extends Document {
  Spreadsheet(long handle) {
    super(handle);
  }

  /** An empty sheet at {@code index} among the sheets, named the first free {@code SheetN}. */
  public Sheet insertSheet(int index) {
    return insertSheet(index, "");
  }

  /**
   * An empty sheet at {@code index} among the sheets. An empty name takes the first free {@code
   * SheetN}; a name another sheet has, ignoring case, is refused.
   */
  public Sheet insertSheet(int index, String name) {
    return new Sheet(insertSheetNative(handle(), index, name), this);
  }

  /** Puts {@code sheet} at {@code index} among the sheets, counted after the move. */
  public void moveSheet(Sheet sheet, int index) {
    try {
      moveSheetNative(handle(), sheet.handle(), index);
    } finally {
      sheet.keepAlive();
    }
  }

  private native long insertSheetNative(long handle, int index, String name);

  private native void moveSheetNative(long handle, long sheetHandle, int index);
}
