package app.opendocument.core;

/**
 * What a view leaves out of the sheet it renders. Mirrors {@code odr::HtmlSheetCut}.
 *
 * @see HtmlView#sheetCut()
 */
public final class HtmlSheetCut {
  /** The extent the sheet's cells span. */
  public TableDimensions content;

  /** The extent the markup carries. */
  public TableDimensions rendered;

  public HtmlSheetCut() {}

  public HtmlSheetCut(TableDimensions content, TableDimensions rendered) {
    this.content = content;
    this.rendered = rendered;
  }

  @Override
  public String toString() {
    return "HtmlSheetCut(content=" + content + ", rendered=" + rendered + ")";
  }
}
