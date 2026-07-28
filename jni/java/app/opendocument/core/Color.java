package app.opendocument.core;

import java.util.Locale;

/** An RGBA color. Mirrors {@code odr::Color}; channels are 0-255. */
public final class Color {
  public final int red;
  public final int green;
  public final int blue;
  public final int alpha;

  public Color(int red, int green, int blue) {
    this(red, green, blue, 255);
  }

  public Color(int red, int green, int blue, int alpha) {
    this.red = red;
    this.green = green;
    this.blue = blue;
    this.alpha = alpha;
  }

  public static Color ofRgb(int rgb) {
    return new Color((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
  }

  public int rgb() {
    return (red << 16) | (green << 8) | blue;
  }

  public int argb() {
    return (alpha << 24) | rgb();
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof Color color && argb() == color.argb();
  }

  @Override
  public int hashCode() {
    return argb();
  }

  @Override
  public String toString() {
    // ROOT, not the default locale: a device set to a locale with its own
    // digits would render this unparseable
    return String.format(Locale.ROOT, "Color(%d, %d, %d, %d)", red, green, blue, alpha);
  }
}
