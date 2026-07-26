package app.opendocument.core;

import java.util.Objects;

/** A quantity: a magnitude and a unit of measure. Mirrors {@code odr::Measure}. */
public final class Measure {
  public final double magnitude;
  public final String unit;

  public Measure(double magnitude, String unit) {
    this.magnitude = magnitude;
    this.unit = Objects.requireNonNull(unit);
  }

  @Override
  public boolean equals(Object other) {
    return other instanceof Measure measure
        && magnitude == measure.magnitude
        && unit.equals(measure.unit);
  }

  @Override
  public int hashCode() {
    return Objects.hash(magnitude, unit);
  }

  @Override
  public String toString() {
    return magnitude + unit;
  }
}
