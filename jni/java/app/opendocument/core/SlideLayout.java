package app.opendocument.core;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * A layout a slide can start from: its name, and the text placeholders a slide made from it
 * holds, in document order. Mirrors {@code odr::SlideLayout}.
 */
public final class SlideLayout {
  public final String name;
  public final List<SlidePlaceholder> placeholders;

  public SlideLayout(String name, List<SlidePlaceholder> placeholders) {
    this.name = Objects.requireNonNull(name);
    this.placeholders = Collections.unmodifiableList(placeholders);
  }

  SlideLayout(String name, SlidePlaceholder[] placeholders) {
    this(name, Arrays.asList(placeholders));
  }

  @Override
  public String toString() {
    return name;
  }
}
