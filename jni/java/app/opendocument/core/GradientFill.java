package app.opendocument.core;

import java.util.Objects;

/** A gradient fill. Mirrors {@code odr::GradientFill}. */
public final class GradientFill {
  public final GradientStyle style;
  /** The colors with their intensities applied. */
  public final Color startColor;
  public final Color endColor;
  /**
   * Degrees, counterclockwise. At 0 a linear gradient runs from the start color at the top to
   * the end color at the bottom.
   */
  public final double angle;
  /** The fraction of the gradient painted in the start color only. */
  public final double border;
  /** The center as fractions of the width and height. */
  public final double centerX;
  public final double centerY;

  GradientFill(
      int style,
      Color startColor,
      Color endColor,
      double angle,
      double border,
      double centerX,
      double centerY) {
    this.style = GradientStyle.fromNative(style);
    this.startColor = Objects.requireNonNull(startColor);
    this.endColor = Objects.requireNonNull(endColor);
    this.angle = angle;
    this.border = border;
    this.centerX = centerX;
    this.centerY = centerY;
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof GradientFill fill
        && style == fill.style
        && startColor.equals(fill.startColor)
        && endColor.equals(fill.endColor)
        && Double.compare(angle, fill.angle) == 0
        && Double.compare(border, fill.border) == 0
        && Double.compare(centerX, fill.centerX) == 0
        && Double.compare(centerY, fill.centerY) == 0;
  }

  @Override
  public int hashCode() {
    return Objects.hash(style, startColor, endColor, angle, border, centerX, centerY);
  }

  @Override
  public String toString() {
    return style + " " + startColor + " - " + endColor + " " + angle + "deg";
  }
}
