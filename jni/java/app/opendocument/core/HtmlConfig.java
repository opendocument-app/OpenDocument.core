package app.opendocument.core;

/**
 * HTML output configuration. Mirrors {@code odr::HtmlConfig}; field defaults
 * match the C++ defaults. The C++ {@code resource_locator} is not exposed; the
 * standard resource locator is always used.
 */
public final class HtmlConfig {
  public String documentOutputFileName = "document.html";

  public String slideOutputFileName = "slide{index}.html";
  public String sheetOutputFileName = "sheet{index}.html";
  public String pageOutputFileName = "page{index}.html";

  public boolean embedImages = true;
  public boolean embedShippedResources = true;

  /** {@code null} keeps the native default (the odr core data path). */
  public String resourcePath;
  public boolean relativeResourcePaths = true;

  public boolean editable = false;

  public boolean textDocumentMargin = false;

  /** {@code null} disables the spreadsheet limit. */
  public TableDimensions spreadsheetLimit = new TableDimensions(10000, 500);
  public boolean spreadsheetLimitByContent = true;
  public HtmlTableGridlines spreadsheetGridlines = HtmlTableGridlines.SOFT;

  /** Initial zoom on mobile. */
  public HtmlViewportMode viewportMode = HtmlViewportMode.AUTOMATIC;
  /** Overrides {@link #viewportMode} for spreadsheet content; {@code null} keeps it. */
  public HtmlViewportMode spreadsheetViewportMode;
  /** Raw {@code content} for the viewport meta tag; overrides the modes above when set. */
  public String viewportContent;

  public boolean formatHtml = false;
  public int htmlIndent = 1;
  public String htmlIndentString = "\t";

  public String backgroundImageFormat = "png";
  public double backgroundImageDpi = 144.0;

  public int pageRangeBegin = 0;
  /** {@code null} renders to the end of the document. */
  public Integer pageRangeEnd;

  public PdfTextMode pdfTextMode = PdfTextMode.DUAL_LAYER;
  public String[] pdfDualLayerFallbackFonts = {
    "Arial", "Helvetica", "Liberation Sans", "DejaVu Sans", "Nimbus Sans"
  };
  public double pdfDualLayerFallbackFontSizeAdjust = 0.5;

  public boolean noDrm = false;

  public boolean embedOutline = false;

  /** {@code null} keeps output in the cache directory. */
  public String outputPath;
}
