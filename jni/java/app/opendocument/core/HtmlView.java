package app.opendocument.core;

/** One view (page/slide/sheet) of an {@link HtmlService}. Mirrors {@code odr::HtmlView}. */
public final class HtmlView extends NativeResource {
  HtmlView(long handle, Object owner) {
    super(handle, owner, HtmlView::destroy);
  }

  public String name() {
    return nameNative(handle());
  }

  public long index() {
    return indexNative(handle());
  }

  public String path() {
    return pathNative(handle());
  }

  public HtmlConfig config() {
    return configNative(handle());
  }

  /**
   * The sheet this view cuts down to {@link HtmlConfig#spreadsheetLimit} and {@link
   * HtmlConfig#spreadsheetCellLimit}, or {@code null} where it writes every cell.
   */
  public HtmlSheetCut sheetCut() {
    return sheetCutNative(handle());
  }

  /** Renders this view; returns the HTML and its resources. */
  public Html.Content writeHtml() {
    return writeHtmlNative(handle());
  }

  /** Renders this view into {@code outputPath} as a self-contained file. */
  public Html bringOffline(String outputPath) {
    return bringOfflineNative(handle(), outputPath);
  }

  private static native void destroy(long handle);

  private native String nameNative(long handle);

  private native long indexNative(long handle);

  private native String pathNative(long handle);

  private native HtmlConfig configNative(long handle);

  private native HtmlSheetCut sheetCutNative(long handle);

  private native Html.Content writeHtmlNative(long handle);

  private native Html bringOfflineNative(long handle, String outputPath);
}
