package app.opendocument.core;

/** Mirrors {@code odr::ElementType}; constant order must match the C++ declaration. */
public enum ElementType {
  NONE,
  ROOT,
  SLIDE,
  SHEET,
  PAGE,
  MASTER_PAGE,
  SHEET_CELL,
  TEXT,
  LINE_BREAK,
  PAGE_BREAK,
  PARAGRAPH,
  SPAN,
  LINK,
  BOOKMARK,
  LIST,
  LIST_ITEM,
  TABLE,
  TABLE_COLUMN,
  TABLE_ROW,
  TABLE_CELL,
  FRAME,
  IMAGE,
  GROUP;

  static ElementType fromNative(int code) {
    return code < 0 ? null : values()[code];
  }

  int toNative() {
    return ordinal();
  }
}
