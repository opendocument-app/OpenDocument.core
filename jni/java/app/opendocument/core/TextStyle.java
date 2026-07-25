package app.opendocument.core;

/** Style of a text run. Mirrors {@code odr::TextStyle}; fields may be {@code null}. */
public final class TextStyle {
  public final String fontName;
  public final Measure fontSize;
  public final FontWeight fontWeight;
  public final FontStyle fontStyle;
  public final Boolean fontUnderline;
  public final Boolean fontLineThrough;
  public final String fontShadow;
  public final Color fontColor;
  public final Color backgroundColor;
  public final FontPosition fontPosition;

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
