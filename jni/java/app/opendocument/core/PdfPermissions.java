package app.opendocument.core;

/**
 * The permission bits ({@code /P}) of an encrypted pdf. Mirrors {@code odr::PdfPermissions}. The
 * renderer enforces them only under {@link HtmlConfig#pdfEnforcePermissions}.
 */
public final class PdfPermissions {
  public final boolean print;
  public final boolean modifyContents;
  public final boolean copy;
  public final boolean modifyAnnotations;
  public final boolean fillForms;
  public final boolean copyForAccessibility;
  public final boolean assemble;
  public final boolean printHighQuality;

  PdfPermissions(
      boolean print,
      boolean modifyContents,
      boolean copy,
      boolean modifyAnnotations,
      boolean fillForms,
      boolean copyForAccessibility,
      boolean assemble,
      boolean printHighQuality) {
    this.print = print;
    this.modifyContents = modifyContents;
    this.copy = copy;
    this.modifyAnnotations = modifyAnnotations;
    this.fillForms = fillForms;
    this.copyForAccessibility = copyForAccessibility;
    this.assemble = assemble;
    this.printHighQuality = printHighQuality;
  }
}
