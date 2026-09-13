package app.opendocument.core;

/**
 * Style of a text run. Mirrors {@code odr::TextStyle}; a {@code null} field is one the document
 * does not state. A caller builds one for {@link Text#setStyle}: every field left {@code null} is
 * left alone on the run.
 */
public final class TextStyle {
  /** Read only: it borrows from the document, and {@link Text#setStyle} refuses it. */
  public String fontName;
  public Measure fontSize;
  public FontWeight fontWeight;
  public FontStyle fontStyle;
  public Boolean fontUnderline;
  public Boolean fontLineThrough;
  public String fontShadow;
  public Color fontColor;
  /** An alpha of 0 takes a highlight away. */
  public Color backgroundColor;
  public FontPosition fontPosition;

  /** Every field {@code null}. */
  public TextStyle() {}

  TextStyle(
      String fontName,
      Measure fontSize,
      int fontWeight,
      int fontStyle,
      Boolean fontUnderline,
      Boolean fontLineThrough,
      String fontShadow,
      Color fontColor,
      Color backgroundColor,
      int fontPosition) {
    this.fontName = fontName;
    this.fontSize = fontSize;
    this.fontWeight = FontWeight.fromNative(fontWeight);
    this.fontStyle = FontStyle.fromNative(fontStyle);
    this.fontUnderline = fontUnderline;
    this.fontLineThrough = fontLineThrough;
    this.fontShadow = fontShadow;
    this.fontColor = fontColor;
    this.backgroundColor = backgroundColor;
    this.fontPosition = FontPosition.fromNative(fontPosition);
  }
}
