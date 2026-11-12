package app.opendocument.core;

import java.util.Objects;

/** A hatch fill. Mirrors {@code odr::HatchFill}. */
public final class HatchFill {
  public final HatchStyle style;
  public final Color color;
  /** The distance between two lines. */
  public final Measure distance;
  /** Degrees, counterclockwise. At 0 the lines run horizontally. */
  public final double rotation;

  HatchFill(int style, Color color, Measure distance, double rotation) {
    this.style = HatchStyle.fromNative(style);
    this.color = Objects.requireNonNull(color);
    this.distance = Objects.requireNonNull(distance);
    this.rotation = rotation;
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof HatchFill fill
        && style == fill.style
        && color.equals(fill.color)
        && distance.equals(fill.distance)
        && Double.compare(rotation, fill.rotation) == 0;
  }

  @Override
  public int hashCode() {
    return Objects.hash(style, color, distance, rotation);
  }

  @Override
  public String toString() {
    return style + " " + color + " " + distance + " " + rotation + "deg";
  }
}
